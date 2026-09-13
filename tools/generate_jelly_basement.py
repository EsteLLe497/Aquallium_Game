"""Generate Route 09's connected basement jellyfish galleries.

Geometry and collision are emitted from the same metre-based specification.
The glTF is authored around the origin; AquariumRenderer places it at the
underwater-arch exit. StageModel mirrors glTF Z and applies its standard
-2.25 m floor offset, followed by a -4.70 m model translation.
"""

from __future__ import annotations

import json
import math
from dataclasses import dataclass
from pathlib import Path

import generate_route_01_02 as route


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_GLB = ROOT / "asset" / "model" / "aquarium_jelly_basement.glb"
OUTPUT_HEADER = ROOT / "src" / "generated" / "JellyBasementGenerated.h"
OUTPUT_MANIFEST = ROOT / "artifacts" / "jelly_basement_manifest.json"

WORLD_ORIGIN = (-10.05, -6.95, 73.0)
ROOM_HEIGHT = 5.5
TANK_RADIUS = 0.85
TANK_WATER_HEIGHT = 3.6
TANKS = (
    ("JB_TANK_01", -4.25, 4.20),
    ("JB_TANK_02", 4.25, 4.20),
    ("JB_TANK_03", 0.00, 8.40),
    ("JB_TANK_04", -4.25, 12.00),
    ("JB_TANK_05", 4.25, 12.00),
)


@dataclass(frozen=True)
class Box:
    name: str
    minimum: tuple[float, float, float]
    maximum: tuple[float, float, float]
    tag: str = "Solid"


@dataclass(frozen=True)
class Circle:
    name: str
    x: float
    z: float
    minimum_y: float
    maximum_y: float
    radius: float
    tag: str = "Solid"


boxes: list[Box] = []
circles: list[Circle] = []
floors: list[dict] = []


def ensure_material(name: str, rgba: tuple[float, float, float, float]) -> None:
    if name not in route.MATERIALS:
        route.MATERIALS[name] = rgba
        route.groups[name] = route.MeshGroup(name=name, material=name)


def reset() -> None:
    ensure_material("ReceptionFloor", (0.055, 0.075, 0.105, 1.0))
    ensure_material("ReceptionWall", (0.018, 0.030, 0.052, 1.0))
    ensure_material("Door", (0.025, 0.045, 0.075, 1.0))
    ensure_material("EmergencyExitDoor", (0.025, 0.045, 0.075, 1.0))
    ensure_material("EmissiveEmergencyGreen", (0.10, 0.82, 0.48, 1.0))
    ensure_material("JellyBackdrop", (0.010, 0.030, 0.075, 1.0))
    ensure_material("JellyFrame", (0.018, 0.038, 0.070, 1.0))
    for group in route.groups.values():
        group.positions.clear()
        group.normals.clear()
        group.texcoords.clear()
        group.indices.clear()
    boxes.clear()
    circles.clear()
    floors.clear()


def authored(point: tuple[float, float, float]) -> tuple[float, float, float]:
    """Convert local runtime coordinates to StageModel-authored coordinates."""
    return point[0], point[1], -point[2]


def world(point: tuple[float, float, float]) -> tuple[float, float, float]:
    return (point[0] + WORLD_ORIGIN[0],
            point[1] + WORLD_ORIGIN[1],
            point[2] + WORLD_ORIGIN[2])


def add_box(material: str, name: str, center, size,
            collider: str | None = None) -> None:
    route.add_box(material, name, authored(center), size)
    if collider:
        cx, cy, cz = world(center)
        sx, sy, sz = size
        boxes.append(Box(name,
                         (cx - sx * 0.5, cy - sy * 0.5, cz - sz * 0.5),
                         (cx + sx * 0.5, cy + sy * 0.5, cz + sz * 0.5),
                         collider))


def add_floor(name: str, center_x: float, center_z: float,
              width: float, depth: float) -> None:
    add_box("ReceptionFloor", name, (center_x, -0.09, center_z),
            (width, 0.18, depth))
    floors.append({
        "name": name,
        "minimum_x": center_x - width * 0.5 + WORLD_ORIGIN[0],
        "maximum_x": center_x + width * 0.5 + WORLD_ORIGIN[0],
        "minimum_z": center_z - depth * 0.5 + WORLD_ORIGIN[2],
        "maximum_z": center_z + depth * 0.5 + WORLD_ORIGIN[2],
        "floor_y": WORLD_ORIGIN[1],
    })


def wall_x(name: str, x0: float, x1: float, z: float,
           y0: float = 0.0, y1: float = ROOM_HEIGHT) -> None:
    add_box("ReceptionWall", name, ((x0 + x1) * 0.5, (y0 + y1) * 0.5, z),
            (x1 - x0, y1 - y0, 0.28), "Solid")


def wall_z(name: str, x: float, z0: float, z1: float,
           y0: float = 0.0, y1: float = ROOM_HEIGHT) -> None:
    add_box("ReceptionWall", name, (x, (y0 + y1) * 0.5, (z0 + z1) * 0.5),
            (0.28, y1 - y0, z1 - z0), "Solid")


def append_quad(material: str, vertices, normal) -> None:
    group = route.groups[material]
    base = len(group.positions) // 3
    for vertex, uv in zip(vertices, ((0, 1), (0, 0), (1, 0), (1, 1))):
        group.positions.extend(authored(vertex))
        group.normals.extend(authored(normal))
        group.texcoords.extend(uv)
    group.indices.extend((base, base + 1, base + 2,
                          base, base + 2, base + 3))


def append_ellipse_strip(material: str, name: str,
                         radius_x: float, radius_z: float,
                         y0: float, y1: float, segments: int = 36) -> None:
    del name
    center_z = 21.0
    for index in range(segments):
        a0 = math.pi * index / segments
        a1 = math.pi * (index + 1) / segments
        p0 = (math.cos(a0) * radius_x, y0,
              center_z + math.sin(a0) * radius_z)
        p1 = (math.cos(a0) * radius_x, y1,
              center_z + math.sin(a0) * radius_z)
        p2 = (math.cos(a1) * radius_x, y1,
              center_z + math.sin(a1) * radius_z)
        p3 = (math.cos(a1) * radius_x, y0,
              center_z + math.sin(a1) * radius_z)
        mid = (a0 + a1) * 0.5
        normal = (-math.cos(mid), 0.0, -math.sin(mid))
        append_quad(material, (p0, p1, p2, p3), normal)


def add_curved_collision(radius_x: float, radius_z: float,
                         segments: int = 36) -> None:
    center_z = 21.0
    for index in range(segments + 1):
        angle = math.pi * index / segments
        local_x = math.cos(angle) * radius_x
        local_z = center_z + math.sin(angle) * radius_z
        wx, _, wz = world((local_x, 0.0, local_z))
        circles.append(Circle(f"JellyPanorama_Glass_{index:02d}", wx, wz,
                              WORLD_ORIGIN[1],
                              WORLD_ORIGIN[1] + ROOM_HEIGHT,
                              0.30, "Glass"))


def add_column_tank(index: int, tank_id: str, x: float, z: float) -> None:
    del tank_id
    # Thirty-two sides stay visually round at this viewing distance while
    # reducing transparent overdraw versus the legacy 48-side columns.
    authored_z = -z
    segments = 32
    base_height = 0.58
    water_bottom = base_height
    water_center = water_bottom + TANK_WATER_HEIGHT * 0.5
    water_top = water_bottom + TANK_WATER_HEIGHT
    route.add_cylinder("TankShell", f"JB_Column_{index:02d}_Base",
                       (x, base_height * 0.5, authored_z),
                       TANK_RADIUS + 0.14, base_height, segments)
    route.add_cylinder("TankWaterJellyCylinder",
                       f"JB_Column_{index:02d}_Water",
                       (x, water_center, authored_z), TANK_RADIUS,
                       TANK_WATER_HEIGHT, segments)
    route.add_cylinder_side("TankGlassJellyCylinder",
                            f"JB_Column_{index:02d}_Glass",
                            (x, water_center, authored_z), TANK_RADIUS + 0.035,
                            TANK_WATER_HEIGHT, segments)
    route.add_cylinder("EmissiveJellyBlue",
                       f"JB_Column_{index:02d}_BottomLight",
                       (x, water_bottom + 0.035, authored_z),
                       TANK_RADIUS * 0.91, 0.07, segments)
    route.add_cylinder("EmissiveJellyBlue",
                       f"JB_Column_{index:02d}_TopLight",
                       (x, water_top - 0.045, authored_z),
                       TANK_RADIUS * 0.86, 0.055, segments)
    route.add_cylinder("TankShell", f"JB_Column_{index:02d}_Top",
                       (x, water_top + 0.12, authored_z),
                       TANK_RADIUS + 0.14, 0.24, segments)
    wx, _, wz = world((x, 0.0, z))
    circles.append(Circle(f"JB_TANK_{index:02d}", wx, wz,
                          WORLD_ORIGIN[1],
                          WORLD_ORIGIN[1] + 4.25,
                          TANK_RADIUS + 0.16, "Glass"))


def build() -> None:
    reset()

    # Column gallery: a direct continuation of the arch with a 6.4 m opening.
    add_floor("JellyColumn_Floor", 0.0, 7.0, 16.0, 14.0)
    add_box("Ceiling", "JellyColumn_Ceiling", (0, ROOM_HEIGHT + 0.09, 7),
            (16, 0.18, 14))
    wall_z("JellyColumn_WestWall", -8.0, 0.0, 14.0)
    wall_z("JellyColumn_EastWall", 8.0, 0.0, 14.0)
    wall_x("JellyColumn_SouthWallLeft", -8.0, -3.2, 0.0)
    wall_x("JellyColumn_SouthWallRight", 3.2, 8.0, 0.0)
    wall_x("JellyColumn_SouthHeader", -3.2, 3.2, 0.0, 4.35, ROOM_HEIGHT)
    wall_x("JellyColumn_NorthWallLeft", -8.0, -2.2, 14.0)
    wall_x("JellyColumn_NorthWallRight", 2.2, 8.0, 14.0)
    wall_x("JellyColumn_NorthHeader", -2.2, 2.2, 14.0, 4.2, ROOM_HEIGHT)

    for index, (tank_id, x, z) in enumerate(TANKS, start=1):
        add_column_tank(index, tank_id, x, z)

    # Curved gallery: D-shaped shell with a 180-degree aquarium wall.
    add_floor("JellyPanorama_Floor", 0.0, 21.0, 22.0, 14.0)
    add_box("Ceiling", "JellyPanorama_Ceiling", (0, ROOM_HEIGHT + 0.09, 21),
            (22, 0.18, 14))
    wall_x("JellyPanorama_SouthWallLeft", -11.0, -2.2, 14.0)
    wall_x("JellyPanorama_SouthWallRight", 2.2, 11.0, 14.0)
    wall_x("JellyPanorama_SouthHeader", -2.2, 2.2, 14.0, 4.2, ROOM_HEIGHT)
    wall_z("JellyPanorama_WestWall", -11.0, 14.0, 21.0)
    wall_z("JellyPanorama_EastWall", 11.0, 14.0, 21.0)

    append_ellipse_strip("JellyBackdrop", "JellyPanorama_Backdrop",
                         10.75, 6.75, 0.0, ROOM_HEIGHT, 36)
    append_ellipse_strip("TankWaterJellyCylinder", "JellyPanorama_Water",
                         10.30, 6.30, 0.34, 3.72, 36)
    append_ellipse_strip("TankGlassJellyCylinder", "JellyPanorama_Glass",
                         10.05, 6.05, 0.34, 3.72, 36)
    append_ellipse_strip("JellyFrame", "JellyPanorama_LowerFrame",
                         9.98, 5.98, 0.0, 0.34, 36)
    append_ellipse_strip("JellyFrame", "JellyPanorama_UpperFrame",
                         9.98, 5.98, 3.72, 4.12, 36)
    append_ellipse_strip("EmissiveJellyBlue", "JellyPanorama_LowerGlow",
                         9.92, 5.92, 0.31, 0.37, 36)
    append_ellipse_strip("EmissiveJellyBlue", "JellyPanorama_UpperGlow",
                         9.92, 5.92, 3.69, 3.75, 36)
    add_curved_collision(9.98, 5.98)

    # The actual emergency exit is a two-leaf sliding door in the east return.
    # A tiny centre seam keeps each leaf independent for the GPU animation.
    for side in (-1, 1):
        add_box("EmergencyExitDoor", "JellyPanorama_FutureExit",
                (10.84, 1.55, 17.4 + side * .589),
                (0.12, 3.10, 1.172))
    wx, wy, wz = world((10.84, 1.55, 17.4))
    boxes.append(Box("JellyPanorama_FutureExit",
                     (wx - .06, wy - 1.55, wz - 1.175),
                     (wx + .06, wy + 1.55, wz + 1.175), "Solid"))
    add_box("EmissiveEmergencyGreen", "JellyPanorama_FutureExitSign",
            (10.75, 3.30, 17.4), (0.08, 0.20, 0.62))


def f(value: float) -> str:
    return f"{0.0 if abs(value) < 0.00005 else value:.5f}f"


def write_header() -> None:
    tags = {"Solid": 2, "Glass": 5}
    OUTPUT_HEADER.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "// Generated by tools/generate_jelly_basement.py. Do not hand edit.",
        "#pragma once", "#include <array>", "#include <cstdint>",
        "namespace jelly_basement", "{",
        "struct BoxSpec { const wchar_t* name; float minX, minY, minZ; float maxX, maxY, maxZ; std::uint8_t tag; };",
        "struct CircleSpec { const wchar_t* name; float x, z, minY, maxY, radius; std::uint8_t tag; };",
        "struct FloorSpec { const wchar_t* name; float minX, maxX, minZ, maxZ, floorY; };",
        f"inline constexpr std::array<BoxSpec, {len(boxes)}> kBoxes{{{{",
    ]
    for box in boxes:
        mn, mx = box.minimum, box.maximum
        lines.append(f'    {{L"{box.name}", {f(mn[0])}, {f(mn[1])}, {f(mn[2])}, '
                     f'{f(mx[0])}, {f(mx[1])}, {f(mx[2])}, {tags[box.tag]}}},')
    lines += ["}};", f"inline constexpr std::array<CircleSpec, {len(circles)}> kCircles{{{{"]
    for circle in circles:
        lines.append(f'    {{L"{circle.name}", {f(circle.x)}, {f(circle.z)}, '
                     f'{f(circle.minimum_y)}, {f(circle.maximum_y)}, '
                     f'{f(circle.radius)}, {tags[circle.tag]}}},')
    lines += ["}};", f"inline constexpr std::array<FloorSpec, {len(floors)}> kFloors{{{{"]
    for floor in floors:
        lines.append(f'    {{L"{floor["name"]}", {f(floor["minimum_x"])}, '
                     f'{f(floor["maximum_x"])}, {f(floor["minimum_z"])}, '
                     f'{f(floor["maximum_z"])}, {f(floor["floor_y"])}}},')
    lines += ["}};", "}"]
    OUTPUT_HEADER.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_manifest(statistics: dict) -> None:
    OUTPUT_MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_MANIFEST.write_text(json.dumps({
        "version": 1,
        "world_origin": WORLD_ORIGIN,
        "column_room": {"width": 16.0, "depth": 14.0,
                        "height": ROOM_HEIGHT, "tank_ids": [t[0] for t in TANKS]},
        "panorama_room": {"width": 22.0, "depth": 14.0,
                          "arc_degrees": 180, "glass_height": 3.38},
        "boxes": [box.__dict__ for box in boxes],
        "circles": [circle.__dict__ for circle in circles],
        "floors": floors,
        "mesh_statistics": statistics,
    }, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    build()
    route.OUTPUT_GLB = OUTPUT_GLB
    stats = route.write_glb()
    route.validate_glb()
    write_header()
    write_manifest(stats)
    print(json.dumps({"glb": str(OUTPUT_GLB), "header": str(OUTPUT_HEADER),
                      "boxes": len(boxes), "circles": len(circles), **stats}))
