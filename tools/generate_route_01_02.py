"""Generate the Entrance and Jellyfish Theater route module as binary glTF.

The model is a replaceable greybox: one unit is one meter, Y is up, interactive
props use stable names, and geometry is grouped by material to keep draw calls
small. Existing aquarium GLBs are never overwritten.
"""

from __future__ import annotations

import json
import math
import struct
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_GLB = ROOT / "model" / "aquarium_route_01_02.glb"
OUTPUT_PLAN = ROOT / "concepts" / "aquarium-route-01-02-plan.png"
OUTPUT_ISOMETRIC = ROOT / "concepts" / "aquarium-route-01-02-isometric.png"


@dataclass
class MeshGroup:
    name: str
    material: str
    positions: list[float] = field(default_factory=list)
    normals: list[float] = field(default_factory=list)
    texcoords: list[float] = field(default_factory=list)
    indices: list[int] = field(default_factory=list)


MATERIALS = {
    "Floor": (0.055, 0.070, 0.085, 1.0),
    "WarmWall": (0.270, 0.220, 0.170, 1.0),
    "DarkWall": (0.025, 0.040, 0.060, 1.0),
    "Ceiling": (0.018, 0.026, 0.040, 1.0),
    "TankShell": (0.020, 0.075, 0.105, 1.0),
    "Metal": (0.155, 0.180, 0.195, 1.0),
    "Furniture": (0.155, 0.105, 0.075, 1.0),
    "Door": (0.310, 0.360, 0.390, 1.0),
    "EmissiveCyan": (0.080, 0.820, 1.000, 1.0),
    "EmissiveWarm": (1.000, 0.420, 0.130, 1.0),
    "EmissiveJellyBlue": (0.018, 0.290, 1.000, 1.0),
    # Water families remain separate so small displays never inherit the hero
    # tank's heavy caustics and scattering shader.
    "TankWaterLarge": (0.010, 0.240, 0.420, 0.62),
    "TankWaterJellyCylinder": (0.025, 0.310, 0.470, 0.20),
    "TankWaterDisplayBox": (0.035, 0.260, 0.390, 0.13),
    "TankGlassJellyCylinder": (0.090, 0.310, 0.470, 0.08),
}

groups = {
    name: MeshGroup(name=name, material=name)
    for name in MATERIALS
}


def add_box(
    material: str,
    name: str,
    center: tuple[float, float, float],
    size: tuple[float, float, float],
) -> None:
    """Append one hard-edged box with face normals and UVs."""

    group = groups[material]
    cx, cy, cz = center
    sx, sy, sz = (value * 0.5 for value in size)
    corners = [
        (cx - sx, cy - sy, cz - sz),
        (cx + sx, cy - sy, cz - sz),
        (cx + sx, cy + sy, cz - sz),
        (cx - sx, cy + sy, cz - sz),
        (cx - sx, cy - sy, cz + sz),
        (cx + sx, cy - sy, cz + sz),
        (cx + sx, cy + sy, cz + sz),
        (cx - sx, cy + sy, cz + sz),
    ]
    faces = [
        ((0, 3, 2, 1), (0.0, 0.0, -1.0)),
        ((4, 5, 6, 7), (0.0, 0.0, 1.0)),
        ((0, 4, 7, 3), (-1.0, 0.0, 0.0)),
        ((1, 2, 6, 5), (1.0, 0.0, 0.0)),
        ((3, 7, 6, 2), (0.0, 1.0, 0.0)),
        ((0, 1, 5, 4), (0.0, -1.0, 0.0)),
    ]
    base = len(group.positions) // 3
    uvs = ((0.0, 0.0), (0.0, 1.0), (1.0, 1.0), (1.0, 0.0))
    for face_index, (corner_indices, normal) in enumerate(faces):
        for corner_index, uv in zip(corner_indices, uvs):
            group.positions.extend(corners[corner_index])
            group.normals.extend(normal)
            group.texcoords.extend(uv)
        vertex = base + face_index * 4
        group.indices.extend(
            (vertex, vertex + 1, vertex + 2, vertex, vertex + 2, vertex + 3)
        )


def add_cylinder(
    material: str,
    name: str,
    center: tuple[float, float, float],
    radius: float,
    height: float,
    segments: int = 32,
) -> None:
    """Append a closed Y-axis cylinder."""

    group = groups[material]
    cx, cy, cz = center
    bottom = cy - height * 0.5
    top = cy + height * 0.5

    # Side strip: independent vertices keep the seam and hard cap normals clean.
    side_base = len(group.positions) // 3
    for index in range(segments + 1):
        angle = math.tau * index / segments
        nx = math.cos(angle)
        nz = math.sin(angle)
        x = cx + nx * radius
        z = cz + nz * radius
        for y, v in ((bottom, 0.0), (top, 1.0)):
            group.positions.extend((x, y, z))
            group.normals.extend((nx, 0.0, nz))
            group.texcoords.extend((index / segments, v))
    for index in range(segments):
        vertex = side_base + index * 2
        group.indices.extend(
            (
                vertex,
                vertex + 1,
                vertex + 3,
                vertex,
                vertex + 3,
                vertex + 2,
            )
        )

    for y, normal_y, reverse in ((top, 1.0, False), (bottom, -1.0, True)):
        cap_base = len(group.positions) // 3
        group.positions.extend((cx, y, cz))
        group.normals.extend((0.0, normal_y, 0.0))
        group.texcoords.extend((0.5, 0.5))
        for index in range(segments):
            angle = math.tau * index / segments
            x = cx + math.cos(angle) * radius
            z = cz + math.sin(angle) * radius
            group.positions.extend((x, y, z))
            group.normals.extend((0.0, normal_y, 0.0))
            group.texcoords.extend(
                (0.5 + math.cos(angle) * 0.5, 0.5 + math.sin(angle) * 0.5)
            )
        for index in range(segments):
            current = cap_base + 1 + index
            following = cap_base + 1 + (index + 1) % segments
            triangle = (
                (cap_base, following, current)
                if reverse
                else (cap_base, current, following)
            )
            group.indices.extend(triangle)


def add_cylinder_side(
    material: str,
    name: str,
    center: tuple[float, float, float],
    radius: float,
    height: float,
    segments: int,
) -> None:
    """Append an open cylinder side for a separate glass interface."""

    group = groups[material]
    cx, cy, cz = center
    bottom = cy - height * 0.5
    top = cy + height * 0.5
    base = len(group.positions) // 3
    for index in range(segments + 1):
        angle = math.tau * index / segments
        nx = math.cos(angle)
        nz = math.sin(angle)
        for y, v in ((bottom, 0.0), (top, 1.0)):
            group.positions.extend((cx + nx * radius, y, cz + nz * radius))
            group.normals.extend((nx, 0.0, nz))
            group.texcoords.extend((index / segments, v))
    for index in range(segments):
        vertex = base + index * 2
        group.indices.extend(
            (vertex, vertex + 1, vertex + 3, vertex, vertex + 3, vertex + 2)
        )


def wall_x(
    material: str,
    name: str,
    x0: float,
    x1: float,
    z: float,
    height: float,
) -> None:
    add_box(
        material,
        name,
        ((x0 + x1) * 0.5, height * 0.5, z),
        (x1 - x0, height, 0.28),
    )


def wall_z(
    material: str,
    name: str,
    x: float,
    z0: float,
    z1: float,
    height: float,
) -> None:
    add_box(
        material,
        name,
        (x, height * 0.5, (z0 + z1) * 0.5),
        (0.28, height, z1 - z0),
    )


def add_jelly_column(
    index: int,
    x: float,
    z: float,
    radius: float,
    water_height: float,
) -> None:
    """Add one slender internally lit cylindrical jellyfish display."""

    base_height = 0.58
    water_bottom = base_height
    water_center = water_bottom + water_height * 0.5
    water_top = water_bottom + water_height
    # The cylinders are viewed at arm's length, so 28 sides exposed obvious
    # facets around the bright caps. Forty-eight remains inexpensive for seven
    # displays while keeping the silhouette visually round.
    segments = 48
    add_cylinder(
        "TankShell",
        f"JellyColumn_{index:02d}_Base",
        (x, base_height * 0.5, z),
        radius + 0.14,
        base_height,
        segments,
    )
    add_cylinder(
        "TankWaterJellyCylinder",
        f"JellyColumn_{index:02d}_Water",
        (x, water_center, z),
        radius,
        water_height,
        segments,
    )
    add_cylinder_side(
        "TankGlassJellyCylinder",
        f"JellyColumn_{index:02d}_Glass",
        (x, water_center, z),
        radius + 0.035,
        water_height,
        segments,
    )
    add_cylinder(
        "EmissiveJellyBlue",
        f"JellyColumn_{index:02d}_BottomLight",
        (x, water_bottom + 0.035, z),
        radius * 0.91,
        0.07,
        segments,
    )
    add_cylinder(
        "EmissiveJellyBlue",
        f"JellyColumn_{index:02d}_TopLight",
        (x, water_top - 0.045, z),
        radius * 0.86,
        0.055,
        segments,
    )
    add_cylinder(
        "TankShell",
        f"JellyColumn_{index:02d}_TopRing",
        (x, water_top + 0.055, z),
        radius + 0.08,
        0.11,
        segments,
    )


def build_layout() -> None:
    # Stage 1: 12 m x 9 m entrance.
    add_box("Floor", "Entrance_Floor", (-12.0, -0.10, 0.0), (12.0, 0.20, 9.0))
    add_box("Ceiling", "Entrance_Ceiling", (-12.0, 4.05, 0.0), (12.0, 0.22, 9.0))
    wall_x("WarmWall", "Entrance_NorthWall", -18.0, -6.0, 4.5, 4.0)
    wall_x("WarmWall", "Entrance_SouthWall", -18.0, -6.0, -4.5, 4.0)
    wall_z("WarmWall", "Entrance_WestWall_North", -18.0, 2.0, 4.5, 4.0)
    wall_z("WarmWall", "Entrance_WestWall_South", -18.0, -4.5, -2.0, 4.0)
    wall_z("WarmWall", "Entrance_EastWall_North", -6.0, 2.0, 4.5, 4.0)
    wall_z("WarmWall", "Entrance_EastWall_South", -6.0, -4.5, -2.0, 4.0)

    # Public automatic doors and recovery panel are separate event-ready pieces.
    add_box("Door", "Entrance_AutomaticDoor_Left", (-17.86, 1.40, -1.0), (0.12, 2.80, 1.90))
    add_box("Door", "Entrance_AutomaticDoor_Right", (-17.86, 1.40, 1.0), (0.12, 2.80, 1.90))
    add_box("Metal", "Entrance_DoorHeader", (-17.78, 3.00, 0.0), (0.34, 0.30, 4.20))
    add_box("EmissiveWarm", "Entrance_EmergencyRelease", (-17.65, 1.25, 2.42), (0.22, 0.46, 0.34))

    # Information counter, map stand, and low benches establish the lobby scale.
    add_box("Furniture", "Entrance_InfoCounter", (-13.6, 0.62, 3.65), (4.8, 1.24, 1.15))
    add_box("WarmWall", "Entrance_InfoBackPanel", (-13.6, 2.25, 4.30), (5.4, 2.45, 0.22))
    add_box("Furniture", "Entrance_InfoSign", (-13.6, 2.95, 4.16), (3.2, 0.12, 0.08))
    add_box("Metal", "Entrance_MapPedestal", (-10.2, 0.72, -0.6), (1.10, 1.44, 0.75))
    add_box("Door", "Entrance_MapBoard", (-10.2, 1.58, -0.6), (1.55, 0.08, 1.05))
    for index, z in enumerate((-2.8, 2.8), start=1):
        add_box("Furniture", f"Entrance_Bench_{index}_Seat", (-8.0, 0.48, z), (2.8, 0.20, 0.72))
        add_box("Metal", f"Entrance_Bench_{index}_Leg", (-8.0, 0.23, z), (2.2, 0.46, 0.18))

    # Three-meter dark vestibule joins the two public zones without a hard cut.
    add_box("Floor", "Vestibule_Floor", (-4.5, -0.10, 0.0), (3.0, 0.20, 4.0))
    add_box("Ceiling", "Vestibule_Ceiling", (-4.5, 3.25, 0.0), (3.0, 0.22, 4.0))
    wall_x("DarkWall", "Vestibule_NorthWall", -6.0, -3.0, 2.0, 3.2)
    wall_x("DarkWall", "Vestibule_SouthWall", -6.0, -3.0, -2.0, 3.2)
    for index, x in enumerate((-5.5, -4.5, -3.5), start=1):
        add_box("TankShell", f"Vestibule_GuideStrip_{index}", (x, 0.16, -1.80), (0.55, 0.08, 0.08))

    # Stage 2: 18 m x 15 m Jellyfish Theater with a higher black ceiling.
    add_box("Floor", "Jellyfish_Floor", (6.0, -0.10, 0.0), (18.0, 0.20, 15.0))
    add_box("Ceiling", "Jellyfish_Ceiling", (6.0, 5.25, 0.0), (18.0, 0.24, 15.0))
    wall_x("DarkWall", "Jellyfish_NorthWall", -3.0, 15.0, 7.5, 5.2)
    wall_x("DarkWall", "Jellyfish_SouthWall", -3.0, 15.0, -7.5, 5.2)
    wall_z("DarkWall", "Jellyfish_WestWall_North", -3.0, 2.0, 7.5, 5.2)
    wall_z("DarkWall", "Jellyfish_WestWall_South", -3.0, -7.5, -2.0, 5.2)
    wall_z("DarkWall", "Jellyfish_EastWall_North", 15.0, 1.6, 7.5, 5.2)
    wall_z("DarkWall", "Jellyfish_EastWall_South", 15.0, -7.5, -1.6, 5.2)

    # A staggered forest of slender columns replaces both the five-meter tank
    # and every rectangular wall display. The center keeps a clear 3 m route.
    jelly_columns = (
        (-0.2, 3.55, 0.72, 3.85),
        (2.0, -3.45, 0.64, 3.35),
        (4.3, 3.20, 0.70, 4.05),
        (6.6, -3.65, 0.74, 3.75),
        (8.8, 3.45, 0.62, 3.40),
        (11.0, -3.25, 0.70, 4.00),
        (13.0, 3.55, 0.66, 3.55),
    )
    for index, (x, z, radius, water_height) in enumerate(
        jelly_columns,
        start=1,
    ):
        add_jelly_column(index, x, z, radius, water_height)

    # The start bench sits outside the south row and faces the column forest.
    add_box("Furniture", "Jellyfish_StartBench_Seat", (-0.3, 0.48, -5.65), (3.2, 0.20, 0.78))
    add_box("Metal", "Jellyfish_StartBench_Leg", (-0.3, 0.23, -5.65), (2.5, 0.46, 0.18))

    # Exit to Stage 3 remains open but visually compressed.
    add_box("Metal", "Jellyfish_ToCoastal_Header", (14.84, 3.35, 0.0), (0.34, 0.42, 3.6))
    add_box("TankShell", "Jellyfish_ToCoastal_Guide", (14.72, 0.14, 0.0), (0.08, 0.08, 2.5))

    # Ceiling fins remain architectural silhouettes. Illumination comes from
    # tank water and tank-integrated rims, not general ceiling fixtures.
    for index, x in enumerate((-1.0, 2.0, 5.0, 8.0, 11.0, 14.0), start=1):
        add_box("Metal", f"Jellyfish_CeilingFin_{index}", (x, 5.04, 0.0), (0.10, 0.08, 10.5))


def pad4(data: bytearray, value: int = 0) -> None:
    while len(data) % 4:
        data.append(value)


def write_glb() -> dict[str, int]:
    binary = bytearray()
    buffer_views: list[dict] = []
    accessors: list[dict] = []
    meshes: list[dict] = []
    nodes: list[dict] = []
    materials: list[dict] = []
    material_indices: dict[str, int] = {}

    for name, rgba in MATERIALS.items():
        material_indices[name] = len(materials)
        emissive = (
            [rgba[0] * 2.0, rgba[1] * 2.0, rgba[2] * 2.0]
            if name.startswith("Emissive")
            else [0.0, 0.0, 0.0]
        )
        materials.append(
            {
                "name": name,
                "pbrMetallicRoughness": {
                    "baseColorFactor": list(rgba),
                    "metallicFactor": 0.55 if name == "Metal" else 0.04,
                    "roughnessFactor": 0.18
                    if name.startswith("TankWater")
                    else (0.28 if name == "Door" else 0.72),
                },
                "emissiveFactor": emissive,
                "alphaMode": (
                    "BLEND"
                    if name.startswith(("TankWater", "TankGlass"))
                    else "OPAQUE"
                ),
                "doubleSided": name.startswith("TankWater"),
            }
        )

    def append_view(payload: bytes, target: int) -> int:
        pad4(binary)
        offset = len(binary)
        binary.extend(payload)
        index = len(buffer_views)
        buffer_views.append(
            {
                "buffer": 0,
                "byteOffset": offset,
                "byteLength": len(payload),
                "target": target,
            }
        )
        return index

    def append_accessor(
        view_index: int,
        component_type: int,
        count: int,
        kind: str,
        minimum=None,
        maximum=None,
    ) -> int:
        accessor = {
            "bufferView": view_index,
            "componentType": component_type,
            "count": count,
            "type": kind,
        }
        if minimum is not None:
            accessor["min"] = minimum
        if maximum is not None:
            accessor["max"] = maximum
        index = len(accessors)
        accessors.append(accessor)
        return index

    for group in groups.values():
        if not group.indices:
            continue
        positions = struct.pack(f"<{len(group.positions)}f", *group.positions)
        normals = struct.pack(f"<{len(group.normals)}f", *group.normals)
        texcoords = struct.pack(f"<{len(group.texcoords)}f", *group.texcoords)
        indices = struct.pack(f"<{len(group.indices)}I", *group.indices)
        position_view = append_view(positions, 34962)
        normal_view = append_view(normals, 34962)
        texcoord_view = append_view(texcoords, 34962)
        index_view = append_view(indices, 34963)
        points = list(
            zip(
                group.positions[0::3],
                group.positions[1::3],
                group.positions[2::3],
            )
        )
        minimum = [min(point[axis] for point in points) for axis in range(3)]
        maximum = [max(point[axis] for point in points) for axis in range(3)]
        vertex_count = len(group.positions) // 3
        position_accessor = append_accessor(
            position_view, 5126, vertex_count, "VEC3", minimum, maximum
        )
        normal_accessor = append_accessor(normal_view, 5126, vertex_count, "VEC3")
        texcoord_accessor = append_accessor(texcoord_view, 5126, vertex_count, "VEC2")
        index_accessor = append_accessor(index_view, 5125, len(group.indices), "SCALAR")
        mesh_index = len(meshes)
        meshes.append(
            {
                "name": group.name,
                "primitives": [
                    {
                        "attributes": {
                            "POSITION": position_accessor,
                            "NORMAL": normal_accessor,
                            "TEXCOORD_0": texcoord_accessor,
                        },
                        "indices": index_accessor,
                        "material": material_indices[group.material],
                        "mode": 4,
                    }
                ],
            }
        )
        nodes.append({"name": group.name, "mesh": mesh_index})

    document = {
        "asset": {"version": "2.0", "generator": "Aquarium Route 01-02 Generator"},
        "scene": 0,
        "scenes": [{"name": "Aquarium_Route_01_02", "nodes": list(range(len(nodes)))}],
        "nodes": nodes,
        "meshes": meshes,
        "materials": materials,
        "accessors": accessors,
        "bufferViews": buffer_views,
        "buffers": [{"byteLength": len(binary)}],
        "extras": {
            "units": "meters",
            "coordinateSystem": "right-handed Y-up",
            "zones": ["Entrance", "Dark Vestibule", "Jellyfish Theater"],
            "entranceDimensions": [12.0, 9.0],
            "jellyfishDimensions": [18.0, 15.0],
            "routeDirection": "+X",
        },
    }
    json_data = bytearray(json.dumps(document, separators=(",", ":")).encode("utf-8"))
    pad4(json_data, 0x20)
    pad4(binary)
    total_length = 12 + 8 + len(json_data) + 8 + len(binary)

    OUTPUT_GLB.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT_GLB.open("wb") as output:
        output.write(struct.pack("<4sII", b"glTF", 2, total_length))
        output.write(struct.pack("<I4s", len(json_data), b"JSON"))
        output.write(json_data)
        output.write(struct.pack("<I4s", len(binary), b"BIN\x00"))
        output.write(binary)

    return {
        "materials": len(materials),
        "meshes": len(meshes),
        "vertices": sum(len(group.positions) // 3 for group in groups.values()),
        "triangles": sum(len(group.indices) // 3 for group in groups.values()),
        "bytes": OUTPUT_GLB.stat().st_size,
    }


def create_plan() -> None:
    width, height = 1600, 900
    image = Image.new("RGB", (width, height), (8, 13, 20))
    draw = ImageDraw.Draw(image)
    scale = 40.0
    origin = (780.0, 450.0)

    def project(x: float, z: float) -> tuple[float, float]:
        return origin[0] + x * scale, origin[1] - z * scale

    def room(rect, fill, outline, title) -> None:
        x0, z0, x1, z1 = rect
        draw.rounded_rectangle(
            (*project(x0, z1), *project(x1, z0)),
            radius=16,
            fill=fill,
            outline=outline,
            width=4,
        )
        draw.multiline_text(
            project((x0 + x1) * 0.5, (z0 + z1) * 0.5),
            title,
            fill=(235, 245, 250),
            anchor="mm",
            align="center",
            spacing=5,
        )

    draw.text((55, 45), "ROUTE 01-02 / ENTRANCE + JELLYFISH THEATER", fill=(235, 245, 250))
    draw.text((55, 78), "1 unit = 1 meter / generated modular GLB", fill=(126, 183, 206))
    room((-18.0, -4.5, -6.0, 4.5), (52, 43, 34), (194, 157, 103), "1  ENTRANCE\n12 m x 9 m")
    room((-6.0, -2.0, -3.0, 2.0), (16, 25, 37), (76, 108, 126), "DARK\nVESTIBULE")
    room((-3.0, -7.5, 15.0, 7.5), (14, 31, 46), (76, 211, 235), "2  JELLYFISH THEATER\n18 m x 15 m")

    # Seven slender cylinders leave the center route unobstructed.
    jelly_columns = (
        (-0.2, 3.55, 0.72),
        (2.0, -3.45, 0.64),
        (4.3, 3.20, 0.70),
        (6.6, -3.65, 0.74),
        (8.8, 3.45, 0.62),
        (11.0, -3.25, 0.70),
        (13.0, 3.55, 0.66),
    )
    for x, z, tank_radius in jelly_columns:
        cx, cy = project(x, z)
        radius = tank_radius * scale
        draw.ellipse(
            (cx - radius, cy - radius, cx + radius, cy + radius),
            fill=(7, 70, 120),
            outline=(62, 139, 255),
            width=4,
        )

    route = [
        project(-18.0, 0.0),
        project(-8.0, 0.0),
        project(-4.5, 0.0),
        project(0.0, 0.0),
        project(4.5, -0.35),
        project(9.0, 0.35),
        project(13.5, 0.0),
        project(15.0, 0.0),
    ]
    draw.line(route, fill=(249, 195, 69), width=7, joint="curve")
    image.save(OUTPUT_PLAN)


def create_isometric() -> None:
    width, height = 1700, 1050
    pixels = np.zeros((height, width, 4), dtype=np.uint8)
    pixels[:, :] = (8, 12, 18, 255)
    depth_buffer = np.full((height, width), np.inf, dtype=np.float32)
    camera = (42.0, 38.0, -49.0)
    target = (-1.5, 1.5, 0.0)

    def normalize(vector):
        length = math.sqrt(sum(value * value for value in vector))
        return tuple(value / length for value in vector)

    def subtract(a, b):
        return tuple(a[index] - b[index] for index in range(3))

    def dot(a, b):
        return sum(a[index] * b[index] for index in range(3))

    def cross(a, b):
        return (
            a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0],
        )

    forward = normalize(subtract(target, camera))
    camera_direction = normalize(subtract(camera, target))
    right = normalize(cross(forward, (0.0, 1.0, 0.0)))
    screen_up = normalize(cross(right, forward))
    light = normalize((-0.35, 0.85, -0.4))
    scale = 24.0

    def project(position):
        relative = subtract(position, target)
        return (
            width * 0.5 + dot(relative, right) * scale,
            height * 0.59 - dot(relative, screen_up) * scale,
            dot(subtract(position, camera), forward),
        )

    triangles = []
    for group_name, group in groups.items():
        # Cutaway preview: the runtime keeps ceilings and complete walls, while
        # this approval image removes the roof and camera-facing wall faces.
        if group_name == "Ceiling":
            continue
        rgba = MATERIALS[group_name]
        base_color = tuple(round(channel * 255) for channel in rgba[:3])
        for offset in range(0, len(group.indices), 3):
            vertex_indices = group.indices[offset : offset + 3]
            vertices = [
                tuple(group.positions[index * 3 + axis] for axis in range(3))
                for index in vertex_indices
            ]
            normal = tuple(
                group.normals[vertex_indices[0] * 3 + axis]
                for axis in range(3)
            )
            if (
                group_name in ("WarmWall", "DarkWall")
                and dot(normal, camera_direction) > 0.18
            ):
                continue
            projected = [project(vertex) for vertex in vertices]
            brightness = 0.34 + max(dot(normal, light), 0.0) * 0.66
            if group_name.startswith("Emissive"):
                brightness = 1.15
            color = tuple(min(255, round(channel * brightness)) for channel in base_color)
            depth = sum(point[2] for point in projected) / 3.0
            triangles.append((depth, projected, (*color, 255)))

    for _, polygon, color in triangles:
        (x0, y0, z0), (x1, y1, z1), (x2, y2, z2) = polygon
        min_x = max(0, int(math.floor(min(x0, x1, x2))))
        max_x = min(width - 1, int(math.ceil(max(x0, x1, x2))))
        min_y = max(0, int(math.floor(min(y0, y1, y2))))
        max_y = min(height - 1, int(math.ceil(max(y0, y1, y2))))
        if min_x > max_x or min_y > max_y:
            continue
        denominator = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(denominator) < 1e-6:
            continue
        yy, xx = np.mgrid[min_y : max_y + 1, min_x : max_x + 1]
        w0 = ((y1 - y2) * (xx - x2) + (x2 - x1) * (yy - y2)) / denominator
        w1 = ((y2 - y0) * (xx - x2) + (x0 - x2) * (yy - y2)) / denominator
        w2 = 1.0 - w0 - w1
        inside = (w0 >= -1e-5) & (w1 >= -1e-5) & (w2 >= -1e-5)
        triangle_depth = w0 * z0 + w1 * z1 + w2 * z2
        local_depth = depth_buffer[min_y : max_y + 1, min_x : max_x + 1]
        visible = inside & (triangle_depth <= local_depth + 1e-4)
        if not np.any(visible):
            continue
        target_pixels = pixels[min_y : max_y + 1, min_x : max_x + 1]
        target_pixels[visible] = color
        local_depth[visible] = triangle_depth[visible]

    image = Image.fromarray(pixels, "RGBA")
    draw = ImageDraw.Draw(image, "RGBA")
    draw.rounded_rectangle((28, 26, 720, 110), radius=14, fill=(5, 9, 14, 225))
    draw.text((50, 45), "ROUTE 01-02 / GENERATED GLB", fill=(235, 244, 248, 255))
    draw.text(
        (50, 77),
        "Entrance 12x9m - dark vestibule - Jellyfish Theater 18x15m",
        fill=(120, 190, 215, 255),
    )
    OUTPUT_ISOMETRIC.parent.mkdir(parents=True, exist_ok=True)
    image.convert("RGB").save(OUTPUT_ISOMETRIC)


def validate_glb() -> None:
    data = OUTPUT_GLB.read_bytes()
    magic, version, total_length = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF" or version != 2 or total_length != len(data):
        raise RuntimeError("Invalid GLB header")
    json_length, json_type = struct.unpack_from("<I4s", data, 12)
    if json_type != b"JSON":
        raise RuntimeError("Missing GLB JSON chunk")
    document = json.loads(data[20 : 20 + json_length].decode("utf-8").rstrip(" "))
    binary_header = 20 + json_length
    binary_length, binary_type = struct.unpack_from("<I4s", data, binary_header)
    if binary_type != b"BIN\x00":
        raise RuntimeError("Missing GLB binary chunk")
    if document["buffers"][0]["byteLength"] > binary_length:
        raise RuntimeError("GLB buffer length exceeds binary chunk")
    for view in document["bufferViews"]:
        end = view.get("byteOffset", 0) + view["byteLength"]
        if end > document["buffers"][0]["byteLength"]:
            raise RuntimeError("GLB buffer view is out of range")


if __name__ == "__main__":
    build_layout()
    statistics = write_glb()
    create_plan()
    create_isometric()
    validate_glb()
    print(
        json.dumps(
            {
                "glb": str(OUTPUT_GLB),
                "plan": str(OUTPUT_PLAN),
                "isometric": str(OUTPUT_ISOMETRIC),
                **statistics,
            },
            ensure_ascii=False,
        )
    )
