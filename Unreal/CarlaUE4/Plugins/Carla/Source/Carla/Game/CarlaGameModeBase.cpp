// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#include "Carla.h"
#include "Carla/Game/CarlaGameModeBase.h"
#include "Map/OccupancyMap.h"
#include "Map/OccupancyTriangle.h"
#include "Carla/OpenDrive/OpenDrive.h"
#include <carla/opendrive/OpenDriveParser.h>
#include "bitmap_image/bitmap_image.hpp"

#include <compiler/disable-ue4-macros.h>
#include <carla/rpc/WeatherParameters.h>
#include <compiler/enable-ue4-macros.h>

ACarlaGameModeBase::ACarlaGameModeBase(const FObjectInitializer& ObjectInitializer)
  : Super(ObjectInitializer)
{
  PrimaryActorTick.bCanEverTick = true;
  PrimaryActorTick.TickGroup = TG_PrePhysics;
  bAllowTickBeforeBeginPlay = false;

  Episode = CreateDefaultSubobject<UCarlaEpisode>(TEXT("Episode"));

  Recorder = CreateDefaultSubobject<ACarlaRecorder>(TEXT("Recorder"));

  CrowdController = CreateDefaultSubobject<ACrowdController>(TEXT("CrowdController"));

  TaggerDelegate = CreateDefaultSubobject<UTaggerDelegate>(TEXT("TaggerDelegate"));
  CarlaSettingsDelegate = CreateDefaultSubobject<UCarlaSettingsDelegate>(TEXT("CarlaSettingsDelegate"));
}

void ACarlaGameModeBase::InitGame(
    const FString &MapName,
    const FString &Options,
    FString &ErrorMessage)
{
  Super::InitGame(MapName, Options, ErrorMessage);

  checkf(
      Episode != nullptr,
      TEXT("Missing episode, can't continue without an episode!"));

#if WITH_EDITOR
    {
      // When playing in editor the map name gets an extra prefix, here we
      // remove it.
      FString CorrectedMapName = MapName;
      constexpr auto PIEPrefix = TEXT("UEDPIE_0_");
      CorrectedMapName.RemoveFromStart(PIEPrefix);
      UE_LOG(LogCarla, Log, TEXT("Corrected map name from %s to %s"), *MapName, *CorrectedMapName);
      Episode->MapName = CorrectedMapName;
    }
#else
  Episode->MapName = MapName;
#endif // WITH_EDITOR

  auto World = GetWorld();
  check(World != nullptr);

  GameInstance = Cast<UCarlaGameInstance>(GetGameInstance());
  checkf(
      GameInstance != nullptr,
      TEXT("GameInstance is not a UCarlaGameInstance, did you forget to set "
           "it in the project settings?"));

  if (TaggerDelegate != nullptr) {
    TaggerDelegate->RegisterSpawnHandler(World);
  } else {
    UE_LOG(LogCarla, Error, TEXT("Missing TaggerDelegate!"));
  }

  if(CarlaSettingsDelegate != nullptr) {
    CarlaSettingsDelegate->ApplyQualityLevelPostRestart();
    CarlaSettingsDelegate->RegisterSpawnHandler(World);
  } else {
    UE_LOG(LogCarla, Error, TEXT("Missing CarlaSettingsDelegate!"));
  }

  if (WeatherClass != nullptr) {
    Episode->Weather = World->SpawnActor<AWeather>(WeatherClass);
  } else {
    UE_LOG(LogCarla, Error, TEXT("Missing weather class!"));
  }

  // Warning: Episode->MapName and MapName are different.
  if (Episode->MapName == TEXT("EmptyMap")) {
    UE_LOG(LogCarla, Display, TEXT("Lane networks available."));
    LaneNetworkActor = World->SpawnActor<ALaneNetworkActor>();
  } else {
    UE_LOG(LogCarla, Display, TEXT("Lane networks unavailable."));
    CreateCarlaOccupancyMap();
    OccupancyMap = &CarlaOccupancyMap;
    CreateCarlaWaypointMap(MapName);
  }

  GameInstance->NotifyInitGame();

  SpawnActorFactories();

  // Dependency injection.
  Recorder->SetEpisode(Episode);
  Episode->SetRecorder(Recorder);
}

void ACarlaGameModeBase::RestartPlayer(AController *NewPlayer)
{
  if (CarlaSettingsDelegate != nullptr)
  {
    CarlaSettingsDelegate->ApplyQualityLevelPreRestart();
  }

  Super::RestartPlayer(NewPlayer);
}

void ACarlaGameModeBase::BeginPlay()
{
  Super::BeginPlay();

  if (true) { /// @todo If semantic segmentation enabled.
    check(GetWorld() != nullptr);
    ATagger::TagActorsInLevel(*GetWorld(), true);
    TaggerDelegate->SetSemanticSegmentationEnabled();
  }

  Episode->InitializeAtBeginPlay();
  //CrowdController->InitializeAtBeginPlay();
  GameInstance->NotifyBeginEpisode(*Episode);

  if (Episode->Weather != nullptr)
  {
    Episode->Weather->ApplyWeather(carla::rpc::WeatherParameters::Default);
  }

  /// @todo Recorder should not tick here, FCarlaEngine should do it.
  // check if replayer is waiting to autostart
  if (Recorder)
  {
    Recorder->GetReplayer()->CheckPlayAfterMapLoaded();
  }
}

void ACarlaGameModeBase::Tick(float DeltaSeconds)
{
  Super::Tick(DeltaSeconds);

  /// @todo Recorder should not tick here, FCarlaEngine should do it.
  if (Recorder) Recorder->Tick(DeltaSeconds);
  //if (CrowdController) CrowdController->Tick(DeltaSeconds);
}

void ACarlaGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
  Episode->EndPlay();
  GameInstance->NotifyEndEpisode();

  Super::EndPlay(EndPlayReason);

  if ((CarlaSettingsDelegate != nullptr) && (EndPlayReason != EEndPlayReason::EndPlayInEditor))
  {
    CarlaSettingsDelegate->Reset();
  }
}

void ACarlaGameModeBase::SpawnActorFactories()
{
  auto *World = GetWorld();
  check(World != nullptr);

  for (auto &FactoryClass : ActorFactories)
  {
    if (FactoryClass != nullptr)
    {
      auto *Factory = World->SpawnActor<ACarlaActorFactory>(FactoryClass);
      if (Factory != nullptr)
      {
        Episode->RegisterActorFactory(*Factory);
        ActorFactoryInstances.Add(Factory);
      }
      else
      {
        UE_LOG(LogCarla, Error, TEXT("Failed to spawn actor spawner"));
      }
    }
  }
}

void ACarlaGameModeBase::CreateCarlaOccupancyMap() {

  // Construct OccupancyMap.
  TArray<FOccupancyTriangle> OccupancyTriangles;
  for (TActorIterator<AStaticMeshActor> ActorItr(GetWorld()); ActorItr; ++ActorItr) {
    if (ActorItr->ActorHasTag(TEXT("Road"))){

      // Written with reference to FStaticMeshSectionAreaWeightedTriangleSampler::GetWeights in
      // Runtime/Engine/Private/StaticMesh.cpp of UE 4.22.

      FStaticMeshLODResources* LODResources = &(ActorItr->GetStaticMeshComponent()->GetStaticMesh()->RenderData->LODResources[0]);
      FIndexArrayView Indices = LODResources->IndexBuffer.GetArrayView();
      const FPositionVertexBuffer& PositionVertexBuffer = LODResources->VertexBuffers.PositionVertexBuffer;

      for (int I = 0; I < Indices.Num(); I += 3) {
        FVector V0 = ActorItr->GetActorLocation() + ActorItr->GetTransform().TransformVector(PositionVertexBuffer.VertexPosition(Indices[I]));
        FVector V1 = ActorItr->GetActorLocation() + ActorItr->GetTransform().TransformVector(PositionVertexBuffer.VertexPosition(Indices[I + 1]));
        FVector V2 = ActorItr->GetActorLocation() + ActorItr->GetTransform().TransformVector(PositionVertexBuffer.VertexPosition(Indices[I + 2]));

        FOccupancyTriangle OccupancyTriangle(
            FVector2D(V0.X, V0.Y),
            FVector2D(V1.X, V1.Y),
            FVector2D(V2.X, V2.Y));

        // Check for precision errors.
        if (OccupancyTriangle.GetBounds().Min.X < -1000000) continue;
        if (OccupancyTriangle.GetBounds().Min.Y < -1000000) continue;
        if (OccupancyTriangle.GetBounds().Max.X > 1000000) continue;
        if (OccupancyTriangle.GetBounds().Max.Y > 1000000) continue;

        OccupancyTriangles.Emplace(OccupancyTriangle);
      }
    }
  }
  CarlaOccupancyMap = FOccupancyMap(OccupancyTriangles);
}
  
void ACarlaGameModeBase::CreateCarlaWaypointMap(const FString& MapName) {
  FString Content = UOpenDrive::LoadXODR(MapName);
  CarlaWaypointMap = carla::opendrive::OpenDriveParser::Load(carla::rpc::FromFString(Content));
  if (!CarlaWaypointMap)
  {
    UE_LOG(LogCarla, Error, TEXT("Failed to parse OpenDrive file."));
  }
}

void ACarlaGameModeBase::RenderOccupancyMap(const FString& FileName) const {
  FOccupancyArea OccupancyArea = OccupancyMap->GetOccupancyArea(
      FBox2D(FVector2D(-20000, -20000), FVector2D(20000, 20000)),
      10, 10);

  cartesian_canvas canvas(OccupancyArea.OccupancyGrid.GetWidth(), OccupancyArea.OccupancyGrid.GetHeight());
  canvas.image().clear(0);
  image_drawer draw(canvas.image());

  draw.pen_color(255, 255, 255);
  for (int Y = 0; Y < OccupancyArea.OccupancyGrid.GetHeight(); Y++) {
    for (int X = 0; X < OccupancyArea.OccupancyGrid.GetWidth(); X++) {
      if (OccupancyArea.OccupancyGrid(X, Y)) {
        draw.plot_pen_pixel(X, Y);
      }
    }
  }

  draw.pen_width(9);
  draw.pen_color(255, 0, 0);
  for (const TArray<FVector2D>& Polygon : OccupancyArea.OffroadPolygons) {
    for (int I = 0; I < Polygon.Num(); I++) {
      FIntPoint Start = OccupancyArea.Point2DToPixel(Polygon[I]);
      FIntPoint End = OccupancyArea.Point2DToPixel(Polygon[(I + 1) % Polygon.Num()]);
      draw.line_segment(Start.X, Start.Y, End.X, End.Y);
    }
  }

  canvas.image().save_image(TCHAR_TO_UTF8(*FileName));
}

void ACarlaGameModeBase::LoadLaneNetwork(const FString& LaneNetworkPath) {
  if (!LaneNetworkActor) {
    UE_LOG(LogCarla, Error, TEXT("Lane networks unavailable for CARLA maps."));
    return;
  }

  LaneNetworkActor->SetLaneNetwork(LaneNetworkPath);
  OccupancyMap = &(LaneNetworkActor->GetOccupancyMap());
}
  
void ACarlaGameModeBase::SpawnWalkers(int Num) {
  const TArray<FActorDefinition>& ActorDefinitions = Episode->GetActorDefinitions();
  TArray<const FActorDefinition*> WalkerActorDefinitions;
  for (const FActorDefinition& ActorDefinition : ActorDefinitions) {
    if (FRegexMatcher(FRegexPattern(TEXT("(|.*,)walker(|,.*)")), ActorDefinition.Tags).FindNext()) {
      WalkerActorDefinitions.Add(&ActorDefinition);
    }
  }

  for (int I = 0; I < Num; I++) {
    FVector2D SpawnPoint = OccupancyMap->RandPoint();
    FTransform Transform(FVector(SpawnPoint.X, SpawnPoint.Y, 300));
    
    const FActorDefinition& ActorDefinition = *WalkerActorDefinitions[FMath::RandRange(0, WalkerActorDefinitions.Num() - 1)];
    FActorDescription ActorDescription;
    ActorDescription.UId = ActorDefinition.UId;
    ActorDescription.Id = ActorDefinition.Id;
    ActorDescription.Class = ActorDefinition.Class;

    AActor* Actor= Episode->SpawnActor(Transform, ActorDescription);
  }
}
