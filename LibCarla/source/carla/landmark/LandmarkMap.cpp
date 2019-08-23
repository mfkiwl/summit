#include "LandmarkMap.h"

#include <osmium/geom/mercator_projection.hpp>
#include <osmium/handler.hpp>
#include <osmium/memory/buffer.hpp>
#include <osmium/visitor.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/index/map/flex_mem.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>

namespace carla {
namespace landmark {

LandmarkMap LandmarkMap::Load(const std::string& file, const geom::Vector2D& offset) {  
  osmium::io::File input_file{file};
  osmium::io::Reader reader{input_file, osmium::osm_entity_bits::node | osmium::osm_entity_bits::way};

  using index_type = osmium::index::map::FlexMem<osmium::unsigned_object_id_type, osmium::Location>;
  using location_handler_type = osmium::handler::NodeLocationsForWays<index_type>;
  index_type index;
  location_handler_type location_handler{index};
  
  struct CountHandler : public osmium::handler::Handler {
    
    geom::Vector2D offset;
    LandmarkMap landmark_map;
    
    void way(const osmium::Way& way) noexcept {
      if (way.tags()["building"]) {
        landmark_map._landmarks.emplace_back();
        for (const osmium::NodeRef it_node : way.nodes()) {
          osmium::geom::Coordinates c = osmium::geom::lonlat_to_mercator(it_node.location());
          landmark_map._landmarks.back().emplace_back(
              static_cast<float>(c.y) + offset.y,  // Swap from Web Mercator -> CARLA.
              static_cast<float>(c.x) + offset.x); // Swap from Web Mercator -> CARLA.
        }
      }
    }

  } handler;
  handler.offset = offset;

  osmium::apply(reader, location_handler, handler);

  std::cerr << handler.landmark_map._landmarks.size() << std::endl;
  return handler.landmark_map;
}
  
std::vector<geom::Vector3D> LandmarkMap::GetMeshTriangles(float height) const {
  std::vector<geom::Vector3D> triangles;

  for (const std::vector<geom::Vector2D> landmark : _landmarks) {
    for (size_t i = 0; i < landmark.size(); i++) {
      const geom::Vector2D& start = landmark[i];
      const geom::Vector2D& end = landmark[(i + 1) % landmark.size()];

      triangles.emplace_back(end.x, end.y, height);
      triangles.emplace_back(end.x, end.y, 0);
      triangles.emplace_back(start.x, start.y, 0);
      
      triangles.emplace_back(start.x, start.y, 0);
      triangles.emplace_back(end.x, end.y, 0);
      triangles.emplace_back(end.x, end.y, height);
      
      triangles.emplace_back(start.x, start.y, 0);
      triangles.emplace_back(start.x, start.y, height);
      triangles.emplace_back(end.x, end.y, height);
      
      triangles.emplace_back(end.x, end.y, height);
      triangles.emplace_back(start.x, start.y, height);
      triangles.emplace_back(start.x, start.y, 0);
    }
  }

  return triangles;
}

}
}
