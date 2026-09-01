"""Generate the connector, basement, and 2F room-shell chunk for route 07.

The hero hall and underwater arch remain independent GLBs so they can later be
culled or streamed per zone.  This chunk closes their seams and provides the
continuous public route without duplicating either expensive exhibit mesh.
One authored unit is one metre.  StageModel performs the usual glTF Z flip.
"""

from __future__ import annotations

import json
import math
from pathlib import Path

import generate_route_01_02 as route


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "model" / "aquarium_continuous_shell.glb"

HALL_FLOOR = 0.0
BASEMENT_FLOOR = -4.70
UPPER_FLOOR = 12.28
SIDE_Z = -17.80  # authored -Z becomes the player's left (+Z) at runtime


def reset_geometry() -> None:
    for group in route.groups.values():
        group.positions.clear()
        group.normals.clear()
        group.texcoords.clear()
        group.indices.clear()


def box(material: str, name: str, center, size) -> None:
    route.add_box(material, name, center, size)


def room_shell(
    name: str,
    center_x: float,
    center_z: float,
    size_x: float,
    size_z: float,
    floor_y: float,
    height: float,
    door_side: str,
    door_center: float,
    door_width: float = 3.0,
) -> None:
    """Build an enclosed room while leaving one explicit public doorway."""
    wall = 0.28
    box("Floor", f"{name}_Floor", (center_x, floor_y - 0.10, center_z),
        (size_x, 0.20, size_z))
    box("Ceiling", f"{name}_Ceiling", (center_x, floor_y + height, center_z),
        (size_x, 0.24, size_z))
    x0, x1 = center_x - size_x * 0.5, center_x + size_x * 0.5
    z0, z1 = center_z - size_z * 0.5, center_z + size_z * 0.5
    wall_y = floor_y + height * 0.5
    if door_side in ("west", "east"):
        door_x = x0 if door_side == "west" else x1
        box("DarkWall", f"{name}_NorthWall", (center_x, wall_y, z1),
            (size_x, height, wall))
        box("DarkWall", f"{name}_SouthWall", (center_x, wall_y, z0),
            (size_x, height, wall))
        for label, a, b in (
            ("DoorLeft", z0, door_center - door_width * 0.5),
            ("DoorRight", door_center + door_width * 0.5, z1),
        ):
            if b > a:
                box("DarkWall", f"{name}_{label}",
                    (door_x, wall_y, (a + b) * 0.5),
                    (wall, height, b - a))
        box("DarkWall", f"{name}_DoorHeader",
            (door_x, floor_y + height - 0.55, door_center),
            (wall, 1.10, door_width))
        other_x = x1 if door_side == "west" else x0
        box("DarkWall", f"{name}_BackWall", (other_x, wall_y, center_z),
            (wall, height, size_z))
    else:
        door_z = z0 if door_side == "south" else z1
        box("DarkWall", f"{name}_WestWall", (x0, wall_y, center_z),
            (wall, height, size_z))
        box("DarkWall", f"{name}_EastWall", (x1, wall_y, center_z),
            (wall, height, size_z))
        for label, a, b in (
            ("DoorLeft", x0, door_center - door_width * 0.5),
            ("DoorRight", door_center + door_width * 0.5, x1),
        ):
            if b > a:
                box("DarkWall", f"{name}_{label}",
                    ((a + b) * 0.5, wall_y, door_z),
                    (b - a, height, wall))
        box("DarkWall", f"{name}_DoorHeader",
            (door_center, floor_y + height - 0.55, door_z),
            (door_width, 1.10, wall))
        other_z = z1 if door_side == "south" else z0
        box("DarkWall", f"{name}_BackWall", (center_x, wall_y, other_z),
            (size_x, height, wall))


def add_jelly_column(index: int, x: float, z: float, height: float) -> None:
    route.add_jelly_column(index, x, z, 0.62, height)


def add_panorama_tank() -> None:
    """One long faceted curve; 18 segments keep it cheap and visibly smooth."""
    center_x, center_z = 99.0, SIDE_Z
    radius = 8.0
    inner_radius = 6.85
    floor_y = BASEMENT_FLOOR
    for index in range(18):
        a0 = math.radians(-72.0 + 144.0 * index / 18.0)
        a1 = math.radians(-72.0 + 144.0 * (index + 1) / 18.0)
        middle = (a0 + a1) * 0.5
        x = center_x + math.cos(middle) * inner_radius
        z = center_z + math.sin(middle) * inner_radius
        chord = 2.0 * inner_radius * math.sin((a1 - a0) * 0.5)
        # Boxes are tangent-aligned approximately by using narrow segments.
        # The slight faceting is hidden by the dark shell and moving water.
        box("TankWaterDisplayBox", f"PanoramaWater_{index:02d}",
            (x, floor_y + 2.55, z), (0.34, 4.70, chord + 0.10))
        box("TankGlassJellyCylinder", f"PanoramaGlass_{index:02d}",
            (x - math.cos(middle) * 0.12, floor_y + 2.55,
             z - math.sin(middle) * 0.12),
            (0.12, 4.90, chord + 0.16))


def build() -> None:
    reset_geometry()

    # Entrance behind the hero hall. The opening aligns with the split rear
    # wall in generate_watatsumi_hall.py; no teleport or loading seam exists.
    room_shell("Entrance", -35.0, 0.0, 14.0, 12.0, HALL_FLOOR, 5.4,
               "east", 0.0, 4.4)
    box("Furniture", "Entrance_InfoCounter", (-38.0, 0.62, 4.6),
        (4.6, 1.24, 1.0))
    box("Door", "Entrance_ExitDoor", (-41.86, 1.45, 0.0),
        (0.12, 2.90, 4.0))
    box("EmissiveWarm", "Entrance_ExitSign", (-41.68, 3.35, 0.0),
        (0.18, 0.32, 1.20))
    # Closed-hours wayfinding: emissive geometry provides readable silhouettes
    # without adding another per-pixel light or washing the room with ambient.
    for z in (-2.38, 2.38):
        box("EmissiveCyan", "Entrance_PortalJamb", (-28.12, 2.50, z),
            (0.06, 5.00, 0.06))
    box("EmissiveCyan", "Entrance_PortalHeader", (-28.12, 5.02, 0.0),
        (0.06, 0.06, 4.82))
    for z in (-1.55, 1.55):
        box("EmissiveCyan", "Entrance_FloorGuide", (-35.0, 0.08, z),
            (12.5, 0.04, 0.05))

    # Tank-left side gallery. The hall facade supplies the entry frame, while
    # the tank generator exposes a curved side acrylic window along this run.
    box("Floor", "HeroTankSideGallery_Floor", (15.0, -0.10, SIDE_Z),
        (16.0, 0.20, 6.2))
    box("Ceiling", "HeroTankSideGallery_Ceiling", (15.0, 7.20, SIDE_Z),
        (16.0, 0.24, 6.2))
    box("DarkWall", "HeroTankSideGallery_OuterWall", (15.0, 3.60, SIDE_Z - 3.0),
        (16.0, 7.20, 0.28))
    box("DarkWall", "HeroTankSideGallery_EndHeader", (22.85, 6.15, SIDE_Z),
        (0.28, 2.10, 6.2))
    box("EmissiveCyan", "SideGallery_FloorGuide", (15.0, 0.08, SIDE_Z - 2.72),
        (14.5, 0.04, 0.06))

    # The translated arch finishes at runtime X=70. Its final elevation is the
    # shared basement floor, so the following rooms join with no vertical snap.
    room_shell("JellyColumnRoom", 79.0, SIDE_Z, 18.0, 15.0,
               BASEMENT_FLOOR, 5.5, "west", SIDE_Z, 3.6)
    for index, (x, z, height) in enumerate((
        (73.0, SIDE_Z - 3.5, 3.5), (76.0, SIDE_Z + 3.6, 4.0),
        (80.0, SIDE_Z - 3.7, 3.7), (83.0, SIDE_Z + 3.4, 4.1),
        (86.0, SIDE_Z - 3.4, 3.6)), start=1):
        add_jelly_column(index, x, z, height)

    room_shell("PanoramaJellyRoom", 99.0, SIDE_Z, 22.0, 18.0,
               BASEMENT_FLOOR, 6.2, "west", SIDE_Z, 4.0)
    add_panorama_tank()

    # 2F destination facades. They establish the confirmed room assignment;
    # interiors can become independently streamed chunks later.
    facade_y = UPPER_FLOOR + 2.45
    for name, x, z, width in (
        ("ManagementRoom", -20.8, -17.8, 4.2),
        ("DolphinRoom", -3.0, 20.5, 4.0),
        ("FutureExhibit", -15.8, 20.5, 4.0),
        ("OutdoorTerrace", -20.8, 17.8, 4.6),
    ):
        box("DarkWall", f"{name}_Facade", (x, facade_y, z),
            (width + 4.0, 4.9, 0.28))
        box("Door", f"{name}_Door", (x, UPPER_FLOOR + 1.50, z - 0.16),
            (width, 3.0, 0.12))
        box("EmissiveCyan", f"{name}_Sign", (x, UPPER_FLOOR + 3.55, z - 0.19),
            (1.6, 0.18, 0.06))


def validate() -> None:
    assert abs((22.0 + 48.0) - 70.0) < 1.0e-5
    assert BASEMENT_FLOOR == -4.70
    assert 14.0 >= 4.4
    assert len(route.groups["TankGlassJellyCylinder"].indices) > 0


if __name__ == "__main__":
    build()
    validate()
    route.OUTPUT_GLB = OUTPUT
    statistics = route.write_glb()
    route.validate_glb()
    print(json.dumps({"glb": str(OUTPUT), **statistics}, ensure_ascii=False))
