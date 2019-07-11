import glob
import math
import os
import sys

sys.path.append(glob.glob('../carla/dist/carla-*%d.%d-%s.egg' % (
    sys.version_info.major,
    sys.version_info.minor,
    'win-amd64' if os.name == 'nt' else 'linux-x86_64'))[0])

import numpy as np
import carla

def rotate(v, radians):
    c, s = math.cos(radians), math.sin(radians)
    return carla.Vector2D(v.x * c - v.y * s, v.x * s + v.y * c)

def from_segment(start, end, width):
    direction = end - start
    direction /= (direction.x**2 + direction.y**2)**0.5
    normal = rotate(direction, math.pi / 2) 

    v1 = start + normal * width / 2.0
    v2 = start - normal * width / 2.0
    v3 = end + normal * width / 2.0
    v4 = end - normal * width / 2.0
    yield (v3, v2, v1)
    yield (v2, v3, v4)

    for i in range(16):
        v1 = start
        v2 = start + rotate(normal, math.pi / 16.0 * i) * width / 2.0
        v3 = start + rotate(normal, math.pi / 16.0 * (i + 1)) * width / 2.0
        yield (v3, v2, v1)

        v1 = end
        v2 = end + rotate(normal, -math.pi / 16.0 * (i + 1)) * width / 2.0
        v3 = end + rotate(normal, -math.pi / 16.0 * i) * width / 2.0
        yield (v3, v2, v1)

if __name__ == '__main__':
    lane_network = carla.LaneNetwork.load('/home/leeyiyuan/Projects/osm-convert/network.ln')
    lanes = [e.data() for e in lane_network.lanes()]
  
    triangles = []
    for lane in lanes:
        triangles.extend(from_segment(
            lane_network.get_lane_start(lane, 0),
            lane_network.get_lane_end(lane, 0),
            lane_network.lane_width()))

    triangles = [[carla.Vector2D(v.x, v.y) for v in t] for t in triangles]
    triangles = [carla.Triangle2D(*t) for t in triangles]
   
    triangles_index = carla.Triangle2DIndex(triangles)

    triangles = triangles_index.query_intersect(
            carla.Vector2D(-200, -200), 
            carla.Vector2D(200, 200))

    vertices = []
    for t in triangles:
        for v in [t.v0, t.v1, t.v2]:
            vertices += [carla.Vector3D(v.x, v.y, 0)]

    client = carla.Client('127.0.0.1', 2000)
    client.set_timeout(2.0)
    world = client.get_world();

    world.spawn_mesh(vertices)
