// Copyright (c) 2019 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "carla/AtomicList.h"
#include "carla/nav/Navigation.h"
#include "carla/NonCopyable.h"
#include "carla/client/Timestamp.h"
#include "carla/client/detail/Client.h"
#include "carla/client/detail/EpisodeProxy.h"
#include "carla/rpc/ActorId.h"

#include <memory>

namespace carla {
namespace client {
namespace detail {

  class Client;
  class EpisodeState;

  class WalkerNavigation
    : public std::enable_shared_from_this<WalkerNavigation>,
      private NonCopyable {
  public:

    explicit WalkerNavigation(Client &client);

    void RegisterWalker(ActorId walker_id, ActorId controller_id) {
      // add to list
      _walkers.Push(WalkerHandle { walker_id, controller_id });
    }

    void AddWalker(ActorId walker_id, carla::geom::Location location) {
      float h = _client.GetWalkerBaseOffset(walker_id) / 100.0f;

      // create the walker in the crowd (to manage its movement in Detour)
      _nav.AddWalker(walker_id, location, h);
    }

    void Tick(const EpisodeState &episode_state);

    // Get Random location in nav mesh
    geom::Location GetRandomLocation() {
      geom::Location random_location(0, 0, 0);
      _nav.GetRandomLocation(random_location, 1.0f);
      return random_location;
    }

    // set a new target point to go
    bool SetWalkerTarget(ActorId id, const carla::geom::Location to) {
      return _nav.SetWalkerTarget(id, to);
    }

    // set new max speed
    bool SetWalkerMaxSpeed(ActorId id, float max_speed) {
      return _nav.SetWalkerMaxSpeed(id, max_speed);
    }

  private:

    Client &_client;

    carla::nav::Navigation _nav;

    struct WalkerHandle {
      ActorId walker;
      ActorId controller;
    };

    AtomicList<WalkerHandle> _walkers;
  };

} // namespace detail
} // namespace client
} // namespace carla