"""Generate the unified V3 aquarium map and its collision specification.

The previous continuous preview assembled three independently-authored GLBs.
That made portal seams, floors and water surfaces overlap.  V3 is authored in
one coordinate system and emits both the render GLB and the C++ collision data
from the same source values.  One authored unit is one metre; StageModel adds
the runtime -2.25 m floor offset and flips glTF Z.
"""

from __future__ import annotations

import json
import math
from dataclasses import dataclass
from pathlib import Path

import generate_route_01_02 as route


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_GLB = ROOT / "asset" / "model" / "aquarium_game_layout_v3.glb"
OUTPUT_HEADER = ROOT / "src" / "generated" / "GameLayoutV3Generated.h"
OUTPUT_MANIFEST = ROOT / "artifacts" / "game_layout_v3_manifest.json"

STAGE_FLOOR_OFFSET = -2.25
FLOOR_1F = 0.0
FLOOR_MID = 2.70
FLOOR_2F = 5.40
FLOOR_B1 = -4.70
HERO_WATER_SURFACE = 4.80


@dataclass(frozen=True)
class BoxCollider:
    name: str
    minimum: tuple[float, float, float]
    maximum: tuple[float, float, float]
    tag: str = "Solid"


@dataclass(frozen=True)
class FloorRect:
    name: str
    minimum_x: float
    maximum_x: float
    minimum_z: float
    maximum_z: float
    floor_y: float


@dataclass(frozen=True)
class PathSpec:
    name: str
    points: tuple[tuple[float, float, float], ...]
    half_width: float


boxes: list[BoxCollider] = []
floors: list[FloorRect] = []
paths: list[PathSpec] = []
portals: list[dict] = []
water_volumes: list[dict] = []
rocks: list[dict] = []


def ensure_material(name: str, rgba: tuple[float, float, float, float]) -> None:
    if name not in route.MATERIALS:
        route.MATERIALS[name] = rgba
        route.groups[name] = route.MeshGroup(name=name, material=name)


def reset_geometry() -> None:
    for group in route.groups.values():
        group.positions.clear()
        group.normals.clear()
        group.texcoords.clear()
        group.indices.clear()
    boxes.clear()
    floors.clear()
    paths.clear()
    portals.clear()
    water_volumes.clear()
    rocks.clear()


def runtime_z(authored_z: float) -> float:
    return -authored_z


def runtime_point(point: tuple[float, float, float]) -> tuple[float, float, float]:
    return point[0], point[1] + STAGE_FLOOR_OFFSET, runtime_z(point[2])


def add_box(material: str, name: str, center, size) -> None:
    route.add_box(material, name, center, size)


def add_collider_box(
    name: str,
    center: tuple[float, float, float],
    size: tuple[float, float, float],
    tag: str = "Solid",
) -> None:
    cx, cy, cz = runtime_point(center)
    sx, sy, sz = size
    boxes.append(BoxCollider(
        name,
        (cx - sx * 0.5, cy - sy * 0.5, cz - sz * 0.5),
        (cx + sx * 0.5, cy + sy * 0.5, cz + sz * 0.5),
        tag,
    ))


def architectural_box(
    material: str,
    name: str,
    center: tuple[float, float, float],
    size: tuple[float, float, float],
    collider_tag: str | None = None,
) -> None:
    add_box(material, name, center, size)
    if collider_tag:
        add_collider_box(name, center, size, collider_tag)


def add_floor(
    name: str,
    x0: float,
    x1: float,
    z0: float,
    z1: float,
    y: float,
    material: str = "Floor",
) -> None:
    assert x1 > x0 and z1 > z0
    add_box(material, name, ((x0 + x1) * 0.5, y - 0.10, (z0 + z1) * 0.5),
            (x1 - x0, 0.20, z1 - z0))
    rz0, rz1 = sorted((runtime_z(z0), runtime_z(z1)))
    floors.append(FloorRect(name, x0, x1, rz0, rz1, y + STAGE_FLOOR_OFFSET))


def add_visual_floor(name: str, x0: float, x1: float,
                     z0: float, z1: float, y: float,
                     material: str = "Floor") -> None:
    """Fill a render seam whose walkable support is authored separately."""
    assert x1 > x0 and z1 > z0
    add_box(material, name,
            ((x0 + x1) * 0.5, y - 0.10, (z0 + z1) * 0.5),
            (x1 - x0, 0.20, z1 - z0))


def add_collision_floor(name: str, x0: float, x1: float,
                        z0: float, z1: float, y: float) -> None:
    rz0, rz1 = sorted((runtime_z(z0), runtime_z(z1)))
    floors.append(FloorRect(name, x0, x1, rz0, rz1, y + STAGE_FLOOR_OFFSET))


def add_ceiling(name: str, x0: float, x1: float, z0: float, z1: float,
                y: float) -> None:
    add_box("Ceiling", name, ((x0 + x1) * 0.5, y, (z0 + z1) * 0.5),
            (x1 - x0, 0.24, z1 - z0))


def add_wall_x(name: str, x0: float, x1: float, z: float,
               floor_y: float, height: float, material: str = "DarkWall",
               tag: str = "Solid") -> None:
    if x1 - x0 <= 0.001:
        return
    center = ((x0 + x1) * 0.5, floor_y + height * 0.5, z)
    size = (x1 - x0, height, 0.28)
    architectural_box(material, name, center, size, tag)


def add_wall_z(name: str, x: float, z0: float, z1: float,
               floor_y: float, height: float, material: str = "DarkWall",
               tag: str = "Solid") -> None:
    if z1 - z0 <= 0.001:
        return
    center = (x, floor_y + height * 0.5, (z0 + z1) * 0.5)
    size = (0.28, height, z1 - z0)
    architectural_box(material, name, center, size, tag)


def add_door_header_x(name: str, x0: float, x1: float, z: float,
                      floor_y: float, room_height: float,
                      door_height: float = 3.2) -> None:
    header_height = room_height - door_height
    if header_height > 0.02:
        add_wall_x(name, x0, x1, z, floor_y + door_height, header_height)


def add_door_header_z(name: str, x: float, z0: float, z1: float,
                      floor_y: float, room_height: float,
                      door_height: float = 3.2) -> None:
    header_height = room_height - door_height
    if header_height > 0.02:
        add_wall_z(name, x, z0, z1, floor_y + door_height, header_height)


def append_quad(material: str, vertices, normal, uvs=((0, 0), (0, 1), (1, 1), (1, 0))):
    group = route.groups[material]
    base = len(group.positions) // 3
    for vertex, uv in zip(vertices, uvs):
        group.positions.extend(vertex)
        group.normals.extend(normal)
        group.texcoords.extend(uv)
    group.indices.extend((base, base + 1, base + 2, base, base + 2, base + 3))


def add_cylinder_between(material: str, start, end, radius: float,
                         segments: int = 8) -> None:
    """Append a small cylinder aligned to an arbitrary 3D segment."""
    dx, dy, dz = (end[index] - start[index] for index in range(3))
    length = math.sqrt(dx * dx + dy * dy + dz * dz)
    if length <= 0.0001:
        return
    axis = (dx / length, dy / length, dz / length)
    reference = (0.0, 1.0, 0.0) if abs(axis[1]) < 0.92 else (1.0, 0.0, 0.0)
    ux = axis[1] * reference[2] - axis[2] * reference[1]
    uy = axis[2] * reference[0] - axis[0] * reference[2]
    uz = axis[0] * reference[1] - axis[1] * reference[0]
    inverse = 1.0 / max(math.sqrt(ux * ux + uy * uy + uz * uz), 0.0001)
    u = (ux * inverse, uy * inverse, uz * inverse)
    v = (axis[1] * u[2] - axis[2] * u[1],
         axis[2] * u[0] - axis[0] * u[2],
         axis[0] * u[1] - axis[1] * u[0])
    ring0, ring1 = [], []
    for index in range(segments):
        angle = math.tau * index / segments
        offset = tuple((u[component] * math.cos(angle) +
                        v[component] * math.sin(angle)) * radius
                       for component in range(3))
        ring0.append(tuple(start[component] + offset[component] for component in range(3)))
        ring1.append(tuple(end[component] + offset[component] for component in range(3)))
    for index in range(segments):
        following = (index + 1) % segments
        append_quad(material,
                    (ring0[index], ring1[index], ring1[following], ring0[following]),
                    (0.0, 1.0, 0.0))


def add_jelly_column_at_floor(index: int, x: float, z: float,
                              floor_y: float, radius: float,
                              water_height: float) -> None:
    base_height = 0.50
    center_y = floor_y + base_height + water_height * 0.5
    route.add_cylinder("TankShell", f"GameV3_Jelly_{index}_Base",
                       (x, floor_y + base_height * 0.5, z),
                       radius + 0.13, base_height, 36)
    route.add_cylinder("TankWaterJellyCylinder", f"GameV3_Jelly_{index}_Water",
                       (x, center_y, z), radius, water_height, 36)
    route.add_cylinder_side("TankGlassJellyCylinder", f"GameV3_Jelly_{index}_Glass",
                            (x, center_y, z), radius + 0.035,
                            water_height, 36)
    route.add_cylinder("EmissiveJellyBlue", f"GameV3_Jelly_{index}_BottomLight",
                       (x, floor_y + base_height + 0.025, z),
                       radius * 0.86, 0.05, 36)
    route.add_cylinder("TankShell", f"GameV3_Jelly_{index}_Top",
                       (x, floor_y + base_height + water_height + 0.08, z),
                       radius + 0.13, 0.16, 36)


def catmull_rom(control_points, subdivisions: int = 12):
    """Sample a smooth curve while preserving the first and last endpoint."""
    result = []
    points = [control_points[0], *control_points, control_points[-1]]
    for segment in range(1, len(points) - 2):
        p0, p1, p2, p3 = points[segment - 1:segment + 3]
        for step in range(subdivisions):
            t = step / subdivisions
            t2, t3 = t * t, t * t * t
            coordinate = tuple(0.5 * (
                2.0 * p1[axis]
                + (-p0[axis] + p2[axis]) * t
                + (2.0 * p0[axis] - 5.0 * p1[axis] + 4.0 * p2[axis] - p3[axis]) * t2
                + (-p0[axis] + 3.0 * p1[axis] - 3.0 * p2[axis] + p3[axis]) * t3
            ) for axis in range(3))
            result.append(coordinate)
    result.append(control_points[-1])
    return result


def cumulative_distances(points):
    distances = [0.0]
    for a, b in zip(points, points[1:]):
        distances.append(distances[-1] + math.dist(a, b))
    return distances


def grade_path(points, start_y: float, end_y: float):
    distances = cumulative_distances(points)
    length = distances[-1]
    graded = []
    for point, distance in zip(points, distances):
        u = distance / max(length, 0.001)
        # C1 smooth landing blend avoids a collision/camera height snap.
        smooth = u * u * (3.0 - 2.0 * u)
        graded.append((point[0], start_y + (end_y - start_y) * smooth, point[2]))
    return graded


def path_edges(points, half_width: float):
    left, right = [], []
    for index, point in enumerate(points):
        before = points[max(0, index - 1)]
        after = points[min(len(points) - 1, index + 1)]
        dx, dz = after[0] - before[0], after[2] - before[2]
        inverse = 1.0 / max(math.hypot(dx, dz), 0.001)
        sx, sz = -dz * inverse * half_width, dx * inverse * half_width
        left.append((point[0] + sx, point[1], point[2] + sz))
        right.append((point[0] - sx, point[1], point[2] - sz))
    return left, right


def add_enclosed_ramp(name: str, control_points, start_y: float, end_y: float,
                      width: float = 4.0, clearance: float = 3.4):
    flat = catmull_rom([(x, 0.0, z) for x, z in control_points], 10)
    center = grade_path(flat, start_y, end_y)
    left, right = path_edges(center, width * 0.5)
    for index in range(len(center) - 1):
        append_quad("Floor", (left[index], right[index], right[index + 1], left[index + 1]), (0, 1, 0))
        l0, l1 = left[index], left[index + 1]
        r0, r1 = right[index], right[index + 1]
        append_quad("DarkWall", (l0, (l0[0], l0[1] + clearance, l0[2]),
                                   (l1[0], l1[1] + clearance, l1[2]), l1), (0, 0, 1))
        append_quad("DarkWall", (r1, (r1[0], r1[1] + clearance, r1[2]),
                                   (r0[0], r0[1] + clearance, r0[2]), r0), (0, 0, -1))
        append_quad("Ceiling", ((l0[0], l0[1] + clearance, l0[2]),
                                 (r0[0], r0[1] + clearance, r0[2]),
                                 (r1[0], r1[1] + clearance, r1[2]),
                                 (l1[0], l1[1] + clearance, l1[2])), (0, -1, 0))
    runtime = tuple(runtime_point(point) for point in center)
    paths.append(PathSpec(name, runtime, width * 0.5 - 0.34))
    return center


def add_arch_tunnel(control_points):
    flat = catmull_rom([(x, 0.0, z) for x, z in control_points], 14)
    center = grade_path(flat, FLOOR_1F, FLOOR_B1)
    half_walk = 3.0
    left, right = path_edges(center, half_walk)
    for index in range(len(center) - 1):
        append_quad("ArchFloor", (left[index], right[index], right[index + 1], left[index + 1]), (0, 1, 0))

    # Curved acrylic shell. Cross-section vertices share a continuous path, so
    # there are no black cracks between separately placed arch modules.
    radius = 3.45
    spring = 1.10
    ring_segments = 12
    rings = []
    for index, point in enumerate(center):
        before = center[max(0, index - 1)]
        after = center[min(len(center) - 1, index + 1)]
        dx, dz = after[0] - before[0], after[2] - before[2]
        inverse = 1.0 / max(math.hypot(dx, dz), 0.001)
        side = (-dz * inverse, dx * inverse)
        ring = []
        for arc in range(ring_segments + 1):
            angle = math.pi - math.pi * arc / ring_segments
            lateral = math.cos(angle) * radius
            height = spring + math.sin(angle) * radius
            ring.append((point[0] + side[0] * lateral, point[1] + height,
                         point[2] + side[1] * lateral))
        rings.append(ring)
    for index in range(len(rings) - 1):
        for arc in range(ring_segments):
            a, b = rings[index][arc], rings[index][arc + 1]
            c, d = rings[index + 1][arc + 1], rings[index + 1][arc]
            append_quad("TankGlassArch", (a, b, c, d), (0, 1, 0))
    for index in range(0, len(rings), 12):
        ring = rings[index]
        for arc in range(ring_segments):
            a, b = ring[arc], ring[arc + 1]
            add_cylinder_between("ArchTrim", a, b, 0.045, 6)

    # Flat water surface follows the tunnel footprint but stays completely west
    # of the opaque vestibule and cannot intersect the hero tank surface.
    surface_center = [(point[0], 5.65, point[2]) for point in center]
    surface_left, surface_right = path_edges(surface_center, 5.2)
    for index in range(len(surface_center) - 1):
        append_quad("ArchWaterSurface", (
            surface_left[index], surface_right[index],
            surface_right[index + 1], surface_left[index + 1]), (0, -1, 0))

    runtime = tuple(runtime_point(point) for point in center)
    paths.append(PathSpec("GameV3_DescendingArch", runtime, half_walk - 0.34))
    return center


def add_hero_tank() -> None:
    hall_height = 8.0
    # Public floor and water end at the same z=-1.5 seam, never overlap.
    add_floor("GameV3_HeroHallFloor", -15.0, 15.0, -1.5, 11.0, FLOOR_1F)
    add_ceiling("GameV3_HeroHallCeiling", -15.0, 15.0, -1.5, 11.0,
                hall_height)
    add_floor("GameV3_HeroTankBed", -12.0, 12.0, -11.0, -1.5, FLOOR_1F, "WatatsumiRock")
    add_box("WatatsumiWater", "GameV3_HeroWater",
            (0.0, HERO_WATER_SURFACE * 0.5, -6.25),
            (23.82, HERO_WATER_SURFACE, 9.32))
    water_volumes.append({"name": "HT01", "min": [-11.91, 0.0, -10.91],
                          "max": [11.91, HERO_WATER_SURFACE, -1.59]})
    architectural_box("WatatsumiGlass", "GameV3_HeroFrontGlass",
                      (0.0, 2.45, -1.52), (24.0, 4.90, 0.10), "Glass")
    architectural_box("WatatsumiGlass", "GameV3_HeroSideGlass",
                      (-12.02, 2.45, -6.25), (0.10, 4.90, 9.50), "Glass")
    add_box("WatatsumiWaterSurface", "GameV3_HeroWaterSurface",
            (0.0, HERO_WATER_SURFACE, -6.25), (23.82, 0.035, 9.32))
    add_wall_x("GameV3_TankBackWall", -12.0, 12.0, -11.0, 0.0, 5.2)
    add_wall_z("GameV3_TankEastWall", 12.0, -11.0, -1.5, 0.0, 5.2)

    # Asymmetrical reef masses remain inside the water AABB by construction.
    for index, (x, y, z, sx, sy, sz) in enumerate((
        (-9.0, 0.8, -8.4, 4.3, 1.6, 3.2),
        (-7.2, 1.9, -7.7, 3.0, 2.2, 2.5),
        (8.9, 1.1, -8.8, 4.0, 2.1, 2.4),
        (7.3, 2.4, -8.0, 2.8, 2.5, 2.2),
        (2.0, 0.55, -7.0, 4.2, 1.0, 2.2),
    )):
        add_box("WatatsumiRock", f"GameV3_Reef_{index}", (x, y, z), (sx, sy, sz))
        rocks.append({"name": f"Reef_{index}", "center": [x, y, z], "size": [sx, sy, sz]})

    # Hall perimeter with explicit openings only.
    add_wall_x("GameV3_HallSouthWest", -15.0, -7.0, 11.0, 0.0, hall_height)
    add_wall_x("GameV3_HallSouthEast", 7.0, 15.0, 11.0, 0.0, hall_height)
    add_door_header_x("GameV3_HallEntranceHeader", -7.0, 7.0, 11.0,
                      0.0, hall_height)
    # The east facade carries two vertically stacked portals: R1 at 1F and
    # R2 at 2F. A single full-height R1 header used to seal the upper landing.
    add_wall_z("GameV3_HallEastNorth", 15.0, -1.5, 5.0,
               0.0, hall_height)
    add_wall_z("GameV3_HallEastUpperPortalLowerWall", 15.0, 5.0, 6.4,
               0.0, 3.0)
    add_wall_z("GameV3_HallEastUpperPortalHeader", 15.0, 5.0, 9.0,
               7.5, hall_height - 7.5)
    add_wall_z("GameV3_HallEastSouth", 15.0, 10.4, 11.0,
               0.0, hall_height)
    add_door_header_z("GameV3_RampPortalHeader", 15.0, 9.0, 10.4,
                      0.0, hall_height)
    # P02 is on the north edge beside the aquarium pane. The west edge is a
    # real perimeter wall; leaving its last 3.7 m open exposed the void outside
    # the hall and looked like a missing wall.
    add_wall_z("GameV3_HallWest", -15.0, -1.5, 11.0,
               0.0, hall_height)
    architectural_box("Furniture", "GameV3_StartBench",
                      (10.0, 0.48, 8.5), (3.2, 0.96, 0.82), "Solid")


def add_entrance_and_return() -> None:
    add_floor("GameV3_EntranceFloor", -7.0, 7.0, 11.0, 21.0, FLOOR_1F)
    add_ceiling("GameV3_EntranceCeiling", -7.0, 7.0, 11.0, 21.0, 4.8)
    add_wall_z("GameV3_EntranceWest", -7.0, 11.0, 21.0, 0.0, 4.8)
    add_wall_z("GameV3_EntranceEastNorth", 7.0, 11.0, 16.0, 0.0, 4.8)
    add_wall_z("GameV3_EntranceEastSouth", 7.0, 20.0, 21.0, 0.0, 4.8)
    add_door_header_z("GameV3_ReturnDoorHeader", 7.0, 16.0, 20.0, 0.0, 4.8)
    add_wall_x("GameV3_EntranceExitWest", -7.0, -2.0, 21.0, 0.0, 4.8)
    add_wall_x("GameV3_EntranceExitEast", 2.0, 7.0, 21.0, 0.0, 4.8)
    add_door_header_x("GameV3_EntranceExitHeader", -2.0, 2.0, 21.0, 0.0, 4.8)
    architectural_box("Door", "GameV3_EntranceLockedDoor",
                      (0.0, 1.45, 20.92), (3.9, 2.9, 0.10), "Trigger")

    # P09 corridor is visible and purposeful: it is the unlocked basement
    # shortcut, not an unexplained dead-end hall.
    add_floor("GameV3_ReturnCorridorFloor", 7.0, 18.0, 16.0, 20.0, FLOOR_1F)
    add_ceiling("GameV3_ReturnCorridorCeiling", 7.0, 18.0, 16.0, 20.0, 3.5)
    add_wall_x("GameV3_ReturnCorridorNorth", 7.0, 18.0, 16.0, 0.0, 3.5)
    add_wall_x("GameV3_ReturnCorridorSouth", 7.0, 18.0, 20.0, 0.0, 3.5)
    portals.append({"id": "P09", "a": "T1_ReturnStair", "b": "Z01_Entrance",
                    "position": [7.0, 0.0, 18.0]})


def add_side_gallery_and_vestibule() -> None:
    add_floor("GameV3_SideGalleryFloor", -16.0, -12.0, -11.0, -1.5, FLOOR_1F)
    add_ceiling("GameV3_SideGalleryCeiling", -16.0, -12.0, -11.0, -1.5, 5.2)
    add_wall_z("GameV3_SideGalleryOuterNorth", -16.0, -11.0, -9.0, 0.0, 5.2)
    add_wall_z("GameV3_SideGalleryOuterSouth", -16.0, -7.0, -1.5, 0.0, 5.2)
    add_door_header_z("GameV3_SideGalleryP03Header", -16.0, -9.0, -7.0, 0.0, 5.2)
    add_wall_x("GameV3_SideGalleryNorthCap", -16.0, -12.0, -11.0, 0.0, 5.2)
    add_wall_x("GameV3_SideGallerySouthShoulder", -16.0, -15.0, -1.5, 0.0, 5.2)
    portals.append({"id": "P02", "a": "Z02_HeroHall", "b": "Z03_SideGallery",
                    "position": [-14.0, 0.0, -1.5]})

    add_floor("GameV3_VestibuleFloor", -24.0, -16.0, -11.0, -5.0, FLOOR_1F)
    add_ceiling("GameV3_VestibuleCeiling", -24.0, -16.0, -11.0, -5.0, 4.2)
    add_wall_x("GameV3_VestibuleNorth", -24.0, -16.0, -11.0, 0.0, 4.2)
    add_wall_x("GameV3_VestibuleSouth", -24.0, -16.0, -5.0, 0.0, 4.2)
    add_wall_z("GameV3_VestibuleEastNorth", -16.0, -11.0, -9.0, 0.0, 4.2)
    add_wall_z("GameV3_VestibuleEastSouth", -16.0, -7.0, -5.0, 0.0, 4.2)
    add_door_header_z("GameV3_VestibuleP03Header", -16.0, -9.0, -7.0, 0.0, 4.2)
    # Match the arch's usable 5.32 m width instead of squeezing it through the
    # former two-metre slit. Narrow jambs and a header form a deliberate frame.
    add_wall_z("GameV3_VestibuleWestNorth", -24.0, -11.0, -10.8, 0.0, 4.2)
    add_wall_z("GameV3_VestibuleWestSouth", -24.0, -5.2, -5.0, 0.0, 4.2)
    add_door_header_z("GameV3_VestibuleP04Header", -24.0, -10.8, -5.2, 0.0, 4.2)
    architectural_box("EmissiveWarm", "GameV3_ArchPortalGuide",
                      (-23.82, 2.4, -8.0), (0.04, 0.16, 1.4))
    portals.extend((
        {"id": "P03", "a": "Z03_SideGallery", "b": "Z04_Vestibule",
         "position": [-16.0, 0.0, -8.0]},
        {"id": "P04", "a": "Z04_Vestibule", "b": "Z05_Arch",
         "position": [-24.0, 0.0, -8.0]},
    ))


def add_basement_rooms() -> None:
    # B1 overlaps the 1F footprint in plan view. Keep every B1 architectural
    # solid below the 1F slab (FLOOR_1F) so an upstairs capsule can never hit
    # a wall that visually belongs to the room below.
    basement_clear_height = 4.55
    # Circular cylinder room: 24 tangent wall segments with exact east/west
    # portal omissions. The floor is one disc, not a stack of overlapping boxes.
    center_x, center_z, radius = -13.0, 15.0, 8.5
    route.add_cylinder("Floor", "GameV3_CylinderRoomFloor",
                       (center_x, FLOOR_B1 - 0.10, center_z), radius, 0.20, 32)
    route.add_cylinder("Ceiling", "GameV3_CylinderRoomCeiling",
                       (center_x, FLOOR_B1 + basement_clear_height, center_z),
                       radius, 0.22, 32)
    # Conservative horizontal strips approximate the circular floor. A single
    # inner square left a large visible disc with an invisible collision edge.
    collision_radius = 8.0
    strip_depth = 1.5
    strip_index = 0
    relative_z = -7.5
    while relative_z < 7.5 - 0.001:
        next_z = min(relative_z + strip_depth, 7.5)
        far_z = max(abs(relative_z), abs(next_z))
        half_x = math.sqrt(max(collision_radius ** 2 - far_z ** 2, 0.0))
        add_collision_floor(
            f"GameV3_CylinderRoomFloor_{strip_index}",
            center_x - half_x, center_x + half_x,
            center_z + relative_z, center_z + next_z, FLOOR_B1)
        strip_index += 1
        relative_z = next_z
    add_collision_floor("GameV3_CylinderWestPortalFloor",
                        -22.5, -18.5, 13.3, 16.7, FLOOR_B1)
    add_collision_floor("GameV3_CylinderEastPortalFloor",
                        -7.5, -4.0, 13.3, 16.7, FLOOR_B1)
    # These bridges used to exist only in collision, so the player crossed a
    # literal visible hole. Render and collision now cover the same hand-off.
    add_visual_floor("GameV3_CylinderWestPortalBridge",
                     -22.5, -18.5, 13.3, 16.7, FLOOR_B1)
    add_visual_floor("GameV3_CylinderEastPortalBridge",
                     -7.5, -4.0, 13.3, 16.7, FLOOR_B1)
    wall_segments = 28
    for index in range(wall_segments):
        angle = math.tau * (index + 0.5) / wall_segments
        # Leave 3.2 m openings at west (arch) and east (panorama).
        if abs(math.sin(angle)) < 0.19 and abs(math.cos(angle)) > 0.92:
            continue
        x = center_x + math.cos(angle) * radius
        z = center_z + math.sin(angle) * radius
        tangent_length = 2.0 * radius * math.sin(math.pi / wall_segments) + 0.04
        # Tangent-oriented walls are emitted as quads; collision uses short
        # conservative AABBs, which overlap each other but never block portals.
        dx, dz = -math.sin(angle) * tangent_length * 0.5, math.cos(angle) * tangent_length * 0.5
        append_quad("DarkWall", (
            (x - dx, FLOOR_B1, z - dz),
            (x - dx, FLOOR_B1 + basement_clear_height, z - dz),
            (x + dx, FLOOR_B1 + basement_clear_height, z + dz),
            (x + dx, FLOOR_B1, z + dz)),
            (math.cos(angle), 0.0, math.sin(angle)))
        add_collider_box(f"GameV3_CylinderWall_{index}",
                         (x, FLOOR_B1 + basement_clear_height * 0.5, z),
                         (abs(dx) * 2.0 + 0.28, basement_clear_height,
                          abs(dz) * 2.0 + 0.28))

    for index, (x, z, height) in enumerate((
        (-16.8, 11.8, 3.5), (-10.0, 11.5, 3.8),
        (-16.5, 18.3, 3.9), (-9.7, 18.0, 3.6),
        (-13.2, 15.0, 3.9),
    )):
        add_jelly_column_at_floor(index, x, z, FLOOR_B1, 0.56, height)
        add_collider_box(f"GameV3_JellyColumn_{index}",
                         (x, FLOOR_B1 + (height + 0.5) * 0.5, z),
                         (1.22, height + 0.5, 1.22), "Glass")

    # Panorama room touches the cylinder-room east portal at x=-4.5/-4.0.
    add_floor("GameV3_PanoramaFloor", -4.0, 18.0, 7.0, 23.0, FLOOR_B1)
    add_ceiling("GameV3_PanoramaCeiling", -4.0, 18.0, 7.0, 23.0,
                FLOOR_B1 + basement_clear_height)
    add_wall_x("GameV3_PanoramaSouth", -4.0, 18.0, 7.0, FLOOR_B1, basement_clear_height)
    add_wall_x("GameV3_PanoramaNorth", -4.0, 18.0, 23.0, FLOOR_B1, basement_clear_height)
    add_wall_z("GameV3_PanoramaWestSouth", -4.0, 7.0, 13.3, FLOOR_B1, basement_clear_height)
    add_wall_z("GameV3_PanoramaWestNorth", -4.0, 16.7, 23.0, FLOOR_B1, basement_clear_height)
    add_door_header_z("GameV3_PanoramaP11Header", -4.0, 13.3, 16.7,
                      FLOOR_B1, basement_clear_height)
    # Two height-separated stair connections share this shaft wall: B1 enters
    # at z=12, while the 1F landing exits at z=19.5. The old single opening at
    # z=15 blocked the actual upper endpoint.
    add_wall_z("GameV3_PanoramaEastSouth", 18.0, 7.0, 10.7,
               FLOOR_B1, basement_clear_height)
    add_door_header_z("GameV3_PanoramaP12Header", 18.0, 10.7, 13.3,
                      FLOOR_B1, basement_clear_height)
    add_wall_z("GameV3_PanoramaEastMiddle", 18.0, 13.3, 18.0,
               FLOOR_B1, basement_clear_height)
    add_wall_z("GameV3_PanoramaEastUpperLandingLowerWall",
               18.0, 18.0, 20.0, FLOOR_B1, 4.68)
    add_wall_z("GameV3_PanoramaEastNorth", 18.0, 20.0, 23.0,
               FLOOR_B1, basement_clear_height)

    # A single concave panoramic tank along the north wall.
    arc_center = (7.0, 15.0)
    inner_radius = 6.8
    outer_radius = 8.0
    segments = 20
    inner_points = []
    outer_points = []
    for index in range(segments + 1):
        angle = math.radians(205.0 + 130.0 * index / segments)
        inner_points.append((arc_center[0] + math.cos(angle) * inner_radius,
                             arc_center[1] + math.sin(angle) * inner_radius))
        outer_points.append((arc_center[0] + math.cos(angle) * outer_radius,
                             arc_center[1] + math.sin(angle) * outer_radius))
    for index in range(segments):
        i0, i1 = inner_points[index], inner_points[index + 1]
        o0, o1 = outer_points[index], outer_points[index + 1]
        append_quad("TankGlassJellyCylinder", (
            (i0[0], FLOOR_B1 + 0.25, i0[1]), (i0[0], FLOOR_B1 + 4.3, i0[1]),
            (i1[0], FLOOR_B1 + 4.3, i1[1]), (i1[0], FLOOR_B1 + 0.25, i1[1])), (0, 0, -1))
        append_quad("TankWaterJellyCylinder", (
            (o0[0], FLOOR_B1 + 0.20, o0[1]), (o0[0], FLOOR_B1 + 4.35, o0[1]),
            (o1[0], FLOOR_B1 + 4.35, o1[1]), (o1[0], FLOOR_B1 + 0.20, o1[1])), (0, 0, -1))
        add_collider_box(
            f"GameV3_PanoramaTankGlass_{index}",
            ((i0[0] + i1[0]) * 0.5, FLOOR_B1 + 2.275,
             (i0[1] + i1[1]) * 0.5),
            (abs(i1[0] - i0[0]) + 0.18, 4.05,
             abs(i1[1] - i0[1]) + 0.18),
            "Glass")
    water_volumes.append({"name": "JELLY_PANORAMA", "min": [-1.0, FLOOR_B1, 7.0],
                          "max": [15.0, FLOOR_B1 + 4.4, 15.0]})
    portals.extend((
        {"id": "P10", "a": "Z05_Arch", "b": "Z06_CylinderJelly",
         "position": [-21.5, FLOOR_B1, 15.0]},
        {"id": "P11", "a": "Z06_CylinderJelly", "b": "Z07_PanoramaJelly",
         "position": [-4.0, FLOOR_B1, 15.0]},
        {"id": "P12", "a": "Z07_PanoramaJelly", "b": "T1_ReturnStair",
         "position": [18.0, FLOOR_B1, 12.0]},
    ))


def add_return_stair() -> None:
    # Four parallel switchback flights fit the 4.7 m rise without reusing the
    # same XZ segment at different heights. The former self-crossing path made
    # floors appear to split and produced ambiguous nearest-segment collision.
    control = [
        (18.0, 12.0), (24.0, 12.0),
        (24.0, 14.5), (18.0, 14.5),
        (18.0, 17.0), (24.0, 17.0),
        (24.0, 19.5), (18.0, 19.5),
    ]
    flat = []
    for a, b in zip(control, control[1:]):
        segment_length = math.dist(a, b)
        count = max(2, math.ceil(segment_length / 0.45))
        for index in range(count):
            u = index / count
            flat.append((a[0] + (b[0] - a[0]) * u, 0.0,
                         a[1] + (b[1] - a[1]) * u))
    flat.append((control[-1][0], 0.0, control[-1][1]))
    center = grade_path(flat, FLOOR_B1, FLOOR_1F)
    left, right = path_edges(center, 1.5)
    for index in range(len(center) - 1):
        append_quad("Floor", (left[index], right[index], right[index + 1], left[index + 1]), (0, 1, 0))
    runtime = tuple(runtime_point(point) for point in center)
    paths.append(PathSpec("GameV3_ReturnStair", runtime, 1.10))
    # Shaft walls have two intentional openings: basement west and 1F west.
    add_wall_x("GameV3_StairSouthWall", 18.0, 26.0, 10.2, FLOOR_B1, 8.2)
    add_wall_x("GameV3_StairNorthWall", 18.0, 26.0, 21.0, FLOOR_B1, 8.2)
    add_wall_z("GameV3_StairEastWall", 26.0, 10.2, 21.0, FLOOR_B1, 8.2)
    add_ceiling("GameV3_StairCeiling", 18.0, 26.0, 10.2, 21.0, 3.5)


def add_ramps_and_upper_floor() -> None:
    r1 = add_enclosed_ramp("GameV3_R1",
        # Finish west-to-east through P06. The previous final tangent pointed
        # away from the gallery and kept the character captured by the ramp.
        [(15.0, 8.4), (20.5, 8.4), (25.0, 4.0), (25.0, -6.0),
         (20.0, -10.0), (16.0, -14.0), (21.0, -14.0)],
        FLOOR_1F, FLOOR_MID, 4.0, 3.5)
    portals.extend((
        {"id": "P05", "a": "Z02_HeroHall", "b": "R1",
         "position": [15.0, FLOOR_1F, 8.4]},
        {"id": "P06", "a": "R1", "b": "ZM_MidGallery",
         "position": [21.0, FLOOR_MID, -14.0]},
    ))
    add_floor("GameV3_MidGalleryFloor", 21.0, 39.0, -17.0, -11.0, FLOOR_MID)
    add_ceiling("GameV3_MidGalleryCeiling", 21.0, 39.0, -17.0, -11.0, FLOOR_MID + 3.7)
    add_wall_x("GameV3_MidGalleryNorth", 21.0, 39.0, -17.0, FLOOR_MID, 3.7)
    add_wall_x("GameV3_MidGallerySouthWest", 21.0, 35.0, -11.0, FLOOR_MID, 3.7)
    add_wall_x("GameV3_MidGallerySouthEast", 38.5, 39.0, -11.0, FLOOR_MID, 3.7)
    add_wall_z("GameV3_MidGalleryWestNorth", 21.0, -17.0, -16.0, FLOOR_MID, 3.7)
    add_wall_z("GameV3_MidGalleryWestSouth", 21.0, -12.0, -11.0, FLOOR_MID, 3.7)
    add_door_header_z("GameV3_MidGalleryP06Header", 21.0, -16.0, -12.0,
                      FLOOR_MID, 3.7)
    add_wall_z("GameV3_MidGalleryEastNorth", 39.0, -17.0, -16.0, FLOOR_MID, 3.7)
    add_wall_z("GameV3_MidGalleryEastSouth", 39.0, -12.0, -11.0, FLOOR_MID, 3.7)
    add_door_header_z("GameV3_MidGalleryP07Header", 39.0, -16.0, -12.0,
                      FLOOR_MID, 3.7)
    for index, x in enumerate((25.0, 30.0, 35.0)):
        add_box("TankWaterDisplayBox", f"GameV3_MidTankWater_{index}",
                (x, FLOOR_MID + 1.65, -16.72), (3.6, 2.55, 0.48))
        architectural_box("TankGlassJellyCylinder", f"GameV3_MidTankGlass_{index}",
                          (x, FLOOR_MID + 1.65, -16.45), (3.8, 2.75, 0.08), "Glass")

    r2 = add_enclosed_ramp("GameV3_R2",
        [(39.0, -14.0), (43.0, -14.0), (47.0, -8.0),
         (47.0, 8.0), (42.0, 13.0), (17.0, 7.0)],
        FLOOR_MID, FLOOR_2F, 4.0, 3.5)
    portals.extend((
        {"id": "P07", "a": "ZM_MidGallery", "b": "R2",
         "position": [39.0, FLOOR_MID, -14.0]},
        {"id": "P08", "a": "R2", "b": "Z10_UpperH",
         "position": [17.0, FLOOR_2F, 7.0]},
    ))

    # H-shaped overlook. Three non-overlapping rectangles meet edge-to-edge.
    add_floor("GameV3_UpperWestArm", -15.0, -11.0, -11.0, 11.0, FLOOR_2F)
    add_floor("GameV3_UpperEastArm", 11.0, 15.0, -11.0, 11.0, FLOOR_2F)
    add_floor("GameV3_UpperCross", -11.0, 11.0, -2.0, 2.0, FLOOR_2F)
    add_floor("GameV3_UpperApproach", 15.0, 42.0, 5.0, 9.0, FLOOR_2F)
    # Outer rails stop exactly at future room door openings.
    for name, center, size in (
        ("UpperWestOuterRail", (-15.0, FLOOR_2F + 0.58, 0.0), (0.12, 1.16, 22.0)),
        ("UpperCrossNorthRail", (0.0, FLOOR_2F + 0.58, -2.0), (22.0, 1.16, 0.12)),
        ("UpperCrossSouthRail", (0.0, FLOOR_2F + 0.58, 2.0), (22.0, 1.16, 0.12)),
    ):
        architectural_box("Metal", f"GameV3_{name}", center, size, "Rail")
    architectural_box("Metal", "GameV3_UpperEastOuterRailSouth",
                      (15.0, FLOOR_2F + 0.58, -3.0),
                      (0.12, 1.16, 16.0), "Rail")
    architectural_box("Metal", "GameV3_UpperEastOuterRailNorth",
                      (15.0, FLOOR_2F + 0.58, 10.0),
                      (0.12, 1.16, 2.0), "Rail")
    architectural_box("Metal", "GameV3_UpperApproachNorthRail",
                      (28.5, FLOOR_2F + 0.58, 5.0),
                      (27.0, 1.16, 0.12), "Rail")
    architectural_box("Metal", "GameV3_UpperApproachSouthRail",
                      (28.5, FLOOR_2F + 0.58, 9.0),
                      (27.0, 1.16, 0.12), "Rail")

    # Future rooms are represented only by explicit facade strips and doors.
    future_doors = (
        ("Management", -13.0, -11.0, 0.0),
        ("Terrace", -13.0, 11.0, 0.0),
        ("Dolphin", 13.0, -11.0, 0.0),
        ("FutureExhibit", 13.0, 11.0, 0.0),
    )
    for name, x, z, _ in future_doors:
        facade_min_x = -15.0 if x < 0.0 else 11.0
        facade_max_x = -11.0 if x < 0.0 else 15.0
        opening_min_x = x - 1.4
        opening_max_x = x + 1.4
        add_wall_x(f"GameV3_{name}FacadeLeft",
                   facade_min_x, opening_min_x, z, FLOOR_2F, 4.0)
        add_wall_x(f"GameV3_{name}FacadeRight",
                   opening_max_x, facade_max_x, z, FLOOR_2F, 4.0)
        add_door_header_x(f"GameV3_{name}FacadeHeader",
                          opening_min_x, opening_max_x, z, FLOOR_2F, 4.0)
        architectural_box("Door", f"GameV3_{name}Door",
                          (x, FLOOR_2F + 1.5, z), (2.76, 3.0, 0.12), "Solid")
        add_box("EmissiveCyan", f"GameV3_{name}Sign",
                (x, FLOOR_2F + 3.35, z), (1.4, 0.16, 0.06))


def add_arch_environment() -> None:
    control = [(-24.0, -8.0), (-34.0, -8.0), (-42.0, 0.0),
               (-34.0, 12.0), (-21.0, 15.0)]
    add_arch_tunnel(control)
    water_volumes.append({"name": "ARCH01", "path": control,
                          "surface_y": 5.65, "half_width": 5.2})


def add_portal_lights() -> None:
    for index, portal in enumerate(portals):
        x, y, z = portal["position"]
        add_box("EmissiveWarm", f"GameV3_PortalLight_{portal['id']}",
                (x, y + 2.75, z), (0.22, 0.16, 0.22))


def build() -> None:
    for name, rgba in {
        "WatatsumiWater": (0.010, 0.135, 0.245, 0.84),
        "WatatsumiGlass": (0.055, 0.220, 0.310, 0.16),
        "WatatsumiWaterSurface": (0.025, 0.300, 0.430, 0.58),
        "WatatsumiRock": (0.025, 0.045, 0.055, 1.0),
        "TankWaterArch": (0.008, 0.120, 0.300, 0.58),
        "TankGlassArch": (0.055, 0.230, 0.400, 0.16),
        "ArchWaterSurface": (0.040, 0.360, 0.560, 0.32),
        "ArchFloor": (0.018, 0.030, 0.045, 1.0),
        "ArchTrim": (0.120, 0.220, 0.285, 1.0),
    }.items():
        ensure_material(name, rgba)
    reset_geometry()
    add_hero_tank()
    add_entrance_and_return()
    add_side_gallery_and_vestibule()
    add_arch_environment()
    add_basement_rooms()
    add_return_stair()
    add_ramps_and_upper_floor()
    add_portal_lights()


def fmt(value: float) -> str:
    value = 0.0 if abs(value) < 0.00005 else value
    return f"{value:.5f}f"


def write_header() -> None:
    OUTPUT_HEADER.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "// Generated by tools/generate_game_layout_v3.py. Do not hand edit.",
        "#pragma once",
        "#include <array>",
        "#include <cstddef>",
        "#include <cstdint>",
        "namespace game_layout_v3",
        "{",
        "struct BoxSpec { const wchar_t* name; float minX, minY, minZ; float maxX, maxY, maxZ; std::uint8_t tag; };",
        "struct FloorSpec { const wchar_t* name; float minX, maxX, minZ, maxZ, floorY; };",
        "struct PathPoint { float x, y, z; };",
        "struct PathSpec { const wchar_t* name; const PathPoint* points; std::size_t count; float halfWidth; };",
        f"inline constexpr std::array<BoxSpec, {len(boxes)}> kBoxes{{{{",
    ]
    tag_values = {"Solid": 2, "Rail": 4, "Glass": 5, "Trigger": 7}
    for collider in boxes:
        mn, mx = collider.minimum, collider.maximum
        lines.append(
            f'    {{L"{collider.name}", {fmt(mn[0])}, {fmt(mn[1])}, {fmt(mn[2])}, '
            f'{fmt(mx[0])}, {fmt(mx[1])}, {fmt(mx[2])}, {tag_values[collider.tag]}}},')
    lines.extend(("}};", f"inline constexpr std::array<FloorSpec, {len(floors)}> kFloors{{{{"))
    for floor in floors:
        lines.append(
            f'    {{L"{floor.name}", {fmt(floor.minimum_x)}, {fmt(floor.maximum_x)}, '
            f'{fmt(floor.minimum_z)}, {fmt(floor.maximum_z)}, {fmt(floor.floor_y)}}},')
    lines.append("}};")
    for index, path in enumerate(paths):
        lines.append(f"inline constexpr std::array<PathPoint, {len(path.points)}> kPath{index}Points{{{{")
        for point in path.points:
            lines.append(f"    {{{fmt(point[0])}, {fmt(point[1])}, {fmt(point[2])}}},")
        lines.append("}};")
    lines.append(f"inline constexpr std::array<PathSpec, {len(paths)}> kPaths{{{{")
    for index, path in enumerate(paths):
        lines.append(
            f'    {{L"{path.name}", kPath{index}Points.data(), kPath{index}Points.size(), {fmt(path.half_width)}}},')
    lines.extend(("}};", "}"))
    OUTPUT_HEADER.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_manifest(statistics: dict) -> None:
    OUTPUT_MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "version": 3,
        "units": "metres",
        "levels": {"B1": FLOOR_B1, "1F": FLOOR_1F, "M": FLOOR_MID, "2F": FLOOR_2F},
        "portals": portals,
        "water_volumes": water_volumes,
        "rocks": rocks,
        "floors": [floor.__dict__ for floor in floors],
        "paths": [{"name": path.name, "half_width": path.half_width,
                   "points": path.points} for path in paths],
        "collider_count": len(boxes),
        "mesh_statistics": statistics,
    }
    OUTPUT_MANIFEST.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")


def validate_build() -> None:
    portal_ids = [portal["id"] for portal in portals]
    assert len(portal_ids) == len(set(portal_ids)), "Portal IDs must be unique"
    assert {"P02", "P03", "P04", "P05", "P06", "P07", "P08", "P09", "P10", "P11", "P12"} <= set(portal_ids)
    assert all(path.half_width >= 0.75 for path in paths)
    hero = next(volume for volume in water_volumes if volume["name"] == "HT01")
    for rock in rocks:
        center, size = rock["center"], rock["size"]
        for axis in range(3):
            assert center[axis] - size[axis] * 0.5 >= hero["min"][axis] - 0.001
            assert center[axis] + size[axis] * 0.5 <= hero["max"][axis] + 0.001
    # Hero and arch water surfaces are separated by at least twelve metres in X.
    assert hero["min"][0] - (-24.0) > 12.0
    assert FLOOR_2F - FLOOR_1F == 5.40
    assert FLOOR_MID - FLOOR_1F == 2.70


if __name__ == "__main__":
    build()
    validate_build()
    route.OUTPUT_GLB = OUTPUT_GLB
    stats = route.write_glb()
    route.validate_glb()
    write_header()
    write_manifest(stats)
    print(json.dumps({
        "glb": str(OUTPUT_GLB),
        "header": str(OUTPUT_HEADER),
        "manifest": str(OUTPUT_MANIFEST),
        "boxes": len(boxes),
        "floors": len(floors),
        "paths": len(paths),
        **stats,
    }, ensure_ascii=False))
