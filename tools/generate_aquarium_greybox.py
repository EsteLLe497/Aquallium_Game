"""Generate a modular aquarium greybox as a self-contained binary glTF.

The output uses meters, Y-up coordinates, named meshes, PBR materials, normals,
UVs, and 32-bit indices. It has no Blender dependency and can be regenerated
after changing the box list below.
"""

from __future__ import annotations

import json
import math
import struct
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_GLB = ROOT / "model" / "aquarium_greybox.glb"
OUTPUT_PREVIEW = ROOT / "concepts" / "aquarium-greybox-plan.png"
OUTPUT_ISOMETRIC = ROOT / "concepts" / "aquarium-greybox-isometric.png"


@dataclass
class MeshGroup:
    name: str
    material: str
    positions: list[float] = field(default_factory=list)
    normals: list[float] = field(default_factory=list)
    texcoords: list[float] = field(default_factory=list)
    indices: list[int] = field(default_factory=list)


GROUP_COLORS = {
    "Floor": (0.11, 0.13, 0.16, 1.0),
    "Concrete": (0.22, 0.25, 0.28, 1.0),
    "TankShell": (0.08, 0.14, 0.18, 1.0),
    "Glass": (0.04, 0.48, 0.68, 0.28),
    "Door": (0.25, 0.20, 0.14, 1.0),
    "Pipe": (0.20, 0.24, 0.26, 1.0),
    "Route": (0.03, 0.58, 0.82, 1.0),
}


groups = {
    name: MeshGroup(name=name, material=name)
    for name in GROUP_COLORS
}


def add_box(
    group_name: str,
    object_name: str,
    center: tuple[float, float, float],
    size: tuple[float, float, float],
) -> None:
    """Append a hard-edged box with per-face normals and UVs."""

    group = groups[group_name]
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
    face_uvs = ((0.0, 0.0), (0.0, 1.0), (1.0, 1.0), (1.0, 0.0))
    for face_index, (corner_indices, normal) in enumerate(faces):
        for corner_index, uv in zip(corner_indices, face_uvs):
            group.positions.extend(corners[corner_index])
            group.normals.extend(normal)
            group.texcoords.extend(uv)
        vertex = base + face_index * 4
        group.indices.extend(
            (vertex, vertex + 1, vertex + 2, vertex, vertex + 2, vertex + 3)
        )


def wall_x(name: str, x0: float, x1: float, z: float, height: float = 4.2) -> None:
    add_box("Concrete", name, ((x0 + x1) * 0.5, height * 0.5, z), (x1 - x0, height, 0.30))


def wall_z(name: str, x: float, z0: float, z1: float, height: float = 4.2) -> None:
    add_box("Concrete", name, (x, height * 0.5, (z0 + z1) * 0.5), (0.30, height, z1 - z0))


def build_layout() -> None:
    # One continuous slab makes collision and later navmesh generation simple.
    add_box("Floor", "Building_Floor", (0.0, -0.10, 0.0), (38.0, 0.20, 32.0))

    # Outer shell. The front entrance is left open between x=-4 and x=4.
    wall_x("Outer_Back", -19.0, 19.0, 16.0)
    wall_x("Outer_Front_Left", -19.0, -4.0, -16.0)
    wall_x("Outer_Front_Right", 4.0, 19.0, -16.0)
    wall_z("Outer_Left", -19.0, -16.0, 16.0)
    wall_z("Outer_Right", 19.0, -16.0, 16.0)

    # Entrance lobby and ticket counter.
    wall_x("Lobby_Back_Left", -19.0, -7.0, -8.5)
    wall_z("Lobby_Right_Segment", -7.0, -16.0, -12.2)
    wall_z("Lobby_Right_Segment_2", -7.0, -9.8, -8.5)
    add_box("Concrete", "Ticket_Counter", (-13.2, 0.65, -11.8), (4.8, 1.30, 1.1))
    add_box("Door", "Emergency_Exit", (-18.80, 1.25, -12.3), (0.18, 2.5, 1.8))

    # Small exhibit room with a wide entrance and three modular wall tanks.
    wall_x("Exhibit_Back", -5.5, 7.0, -8.5)
    wall_z("Exhibit_Left", -5.5, -15.0, -12.0)
    wall_z("Exhibit_Left_2", -5.5, -10.0, -8.5)
    wall_z("Exhibit_Right", 7.0, -15.0, -12.0)
    wall_z("Exhibit_Right_2", 7.0, -10.0, -8.5)
    for index, x in enumerate((-2.8, 0.6, 4.0), start=1):
        add_box("Glass", f"Exhibit_Tank_{index}", (x, 1.65, -8.30), (2.7, 2.2, 0.18))
        add_box("TankShell", f"Exhibit_Base_{index}", (x, 0.42, -8.18), (2.9, 0.84, 0.55))

    # Main tank: open interior volume, opaque shell, and separate front glass.
    add_box("TankShell", "MainTank_Bottom", (0.0, 0.18, 3.0), (30.0, 0.36, 9.0))
    add_box("TankShell", "MainTank_Back", (0.0, 4.0, 7.35), (30.0, 8.0, 0.30))
    add_box("TankShell", "MainTank_Left", (-15.0, 4.0, 3.0), (0.30, 8.0, 9.0))
    add_box("TankShell", "MainTank_Right", (15.0, 4.0, 3.0), (0.30, 8.0, 9.0))
    add_box("TankShell", "MainTank_Ceiling", (0.0, 8.0, 3.0), (30.0, 0.30, 9.0))
    add_box("Glass", "MainTank_ViewingGlass", (0.0, 4.0, -1.45), (29.7, 7.6, 0.18))
    add_box("TankShell", "MainTank_LowerFrame", (0.0, 0.45, -1.62), (30.5, 0.9, 0.45))
    add_box("TankShell", "MainTank_UpperFrame", (0.0, 7.75, -1.62), (30.5, 0.50, 0.45))

    # Viewing corridor boundary leaves a 3 m opening into the entrance side.
    wall_x("ViewingCorridor_Front_Left", -19.0, -8.5, -8.5)
    wall_x("ViewingCorridor_Front_Middle", -5.5, 7.0, -8.5)
    wall_x("ViewingCorridor_Front_Right", 10.0, 19.0, -8.5)

    # Control room overlooking the rear/side of the main tank.
    wall_z("ControlRoom_Right", -10.0, 8.0, 16.0)
    wall_x("ControlRoom_Front_Left", -19.0, -16.2, 8.0)
    wall_x("ControlRoom_Front_Right", -13.8, -10.0, 8.0)
    add_box("Glass", "ControlRoom_Window", (-12.0, 2.2, 8.15), (3.6, 2.0, 0.12))
    add_box("Concrete", "Control_Desk", (-14.4, 0.65, 12.0), (5.2, 1.3, 1.2))

    # Maintenance corridor and pump room behind the tank.
    wall_x("Maintenance_Back", -10.0, 10.0, 15.7)
    wall_z("PumpRoom_Left_Segment", 10.0, 8.0, 11.8)
    wall_z("PumpRoom_Left_Segment_2", 10.0, 14.2, 16.0)
    for index, z in enumerate((10.0, 12.2, 14.4), start=1):
        add_box("Pipe", f"Pump_Header_{index}", (14.2, 2.7, z), (6.5, 0.28, 0.28))
        add_box("Pipe", f"Pump_Riser_{index}", (12.0 + index * 1.6, 1.5, z), (0.28, 2.8, 0.28))
    add_box("Door", "PumpRoom_Door", (10.0, 1.25, 13.0), (0.18, 2.5, 1.8))

    # Chase/break location. The glass is a separate object for event swapping.
    wall_z("ChaseRoom_Left", 9.5, -16.0, -12.0)
    wall_z("ChaseRoom_Left_2", 9.5, -10.0, -8.5)
    add_box("Glass", "BreakEvent_Glass_Intact", (18.72, 2.1, -11.8), (0.16, 3.6, 5.0))
    add_box("TankShell", "BreakEvent_Frame_Top", (18.55, 4.05, -11.8), (0.42, 0.32, 5.5))
    add_box("TankShell", "BreakEvent_Frame_Bottom", (18.55, 0.25, -11.8), (0.42, 0.50, 5.5))

    # Doors are authored separately so gameplay can hide/animate them.
    door_specs = [
        ("Lobby_To_Viewing_Door", -7.0, 1.25, -11.0, 0.18, 2.5, 1.8),
        ("Viewing_To_Exhibit_Door", 7.0, 1.25, -11.0, 0.18, 2.5, 1.8),
        ("Viewing_To_Chase_Door", 9.5, 1.25, -11.0, 0.18, 2.5, 1.8),
        ("ControlRoom_Door", -15.0, 1.25, 8.0, 1.8, 2.5, 0.18),
    ]
    for name, x, y, z, sx, sy, sz in door_specs:
        add_box("Door", name, (x, y, z), (sx, sy, sz))

    # Low route strips are Blender-only visual guides and can be deleted later.
    route_segments = [
        ((-12.0, 0.025, -12.0), (8.0, 0.05, 0.18)),
        ((-8.0, 0.025, -9.5), (0.18, 0.05, 5.0)),
        ((0.0, 0.025, -5.2), (27.0, 0.05, 0.18)),
        ((13.0, 0.025, -9.0), (0.18, 0.05, 7.0)),
        ((14.0, 0.025, 12.0), (0.18, 0.05, 6.0)),
        ((0.0, 0.025, 13.5), (20.0, 0.05, 0.18)),
    ]
    for index, (center, size) in enumerate(route_segments, start=1):
        add_box("Route", f"PlayerRoute_{index}", center, size)


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

    for name, rgba in GROUP_COLORS.items():
        material_indices[name] = len(materials)
        materials.append(
            {
                "name": name,
                "pbrMetallicRoughness": {
                    "baseColorFactor": list(rgba),
                    "metallicFactor": 0.05 if name != "Pipe" else 0.55,
                    "roughnessFactor": 0.68 if name != "Glass" else 0.12,
                },
                "alphaMode": "BLEND" if name == "Glass" else "OPAQUE",
                "doubleSided": name == "Glass",
            }
        )

    def append_view(payload: bytes, target: int) -> int:
        pad4(binary)
        offset = len(binary)
        binary.extend(payload)
        view_index = len(buffer_views)
        buffer_views.append(
            {
                "buffer": 0,
                "byteOffset": offset,
                "byteLength": len(payload),
                "target": target,
            }
        )
        return view_index

    def append_accessor(
        view_index: int,
        component_type: int,
        count: int,
        accessor_type: str,
        minimum: list[float] | None = None,
        maximum: list[float] | None = None,
    ) -> int:
        accessor = {
            "bufferView": view_index,
            "componentType": component_type,
            "count": count,
            "type": accessor_type,
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
        points = list(zip(group.positions[0::3], group.positions[1::3], group.positions[2::3]))
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
        "asset": {"version": "2.0", "generator": "Aquarium Greybox Generator"},
        "scene": 0,
        "scenes": [{"name": "Aquarium_Greybox", "nodes": list(range(len(nodes)))}],
        "nodes": nodes,
        "meshes": meshes,
        "materials": materials,
        "accessors": accessors,
        "bufferViews": buffer_views,
        "buffers": [{"byteLength": len(binary)}],
        "extras": {
            "units": "meters",
            "coordinateSystem": "right-handed Y-up",
            "mainTankDimensions": [30.0, 8.0, 9.0],
            "zones": [
                "Entrance Lobby",
                "Main Tank Viewing Corridor",
                "Small Exhibit Room",
                "Control Room",
                "Maintenance Corridor",
                "Pump Room",
                "Glass Break Chase Area",
            ],
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
        "nodes": len(nodes),
        "meshes": len(meshes),
        "vertices": sum(len(group.positions) // 3 for group in groups.values()),
        "triangles": sum(len(group.indices) // 3 for group in groups.values()),
        "bytes": OUTPUT_GLB.stat().st_size,
    }


def create_preview() -> None:
    width, height = 1400, 1000
    image = Image.new("RGB", (width, height), (12, 16, 22))
    draw = ImageDraw.Draw(image)
    scale = 23.0
    origin = (width * 0.5, height * 0.53)

    def project(x: float, z: float) -> tuple[float, float]:
        return origin[0] + x * scale, origin[1] - z * scale

    def room(rect: tuple[float, float, float, float], fill, outline, label: str) -> None:
        x0, z0, x1, z1 = rect
        draw.rectangle((*project(x0, z1), *project(x1, z0)), fill=fill, outline=outline, width=3)
        center = project((x0 + x1) * 0.5, (z0 + z1) * 0.5)
        draw.text(center, label, fill=(235, 242, 248), anchor="mm")

    room((-19, -16, -7, -8.5), (44, 39, 34), (150, 130, 100), "1 ENTRANCE")
    room((-19, -8.5, 19, -1.5), (17, 31, 43), (50, 165, 205), "2 MAIN TANK VIEWING")
    room((-5.5, -16, 7, -8.5), (24, 39, 49), (60, 155, 190), "3 EXHIBIT")
    room((-19, 8, -10, 16), (35, 42, 48), (135, 150, 160), "4 CONTROL")
    room((-10, 7.5, 10, 16), (28, 32, 37), (130, 140, 150), "5 MAINTENANCE")
    room((10, 7.5, 19, 16), (45, 40, 30), (180, 135, 65), "6 PUMP")
    room((9.5, -16, 19, -8.5), (48, 27, 30), (210, 75, 80), "7 CHASE / BREAK")
    room((-15, -1.5, 15, 7.5), (6, 82, 116), (50, 205, 245), "MAIN TANK 30m x 9m x 8m")

    route = [
        project(-13, -14),
        project(-9, -10),
        project(-9, -5),
        project(13, -5),
        project(14, -12),
        project(14, 12),
        project(-14, 12),
    ]
    draw.line(route, fill=(70, 210, 245), width=7, joint="curve")
    for point in route:
        draw.ellipse((point[0] - 6, point[1] - 6, point[0] + 6, point[1] + 6), fill=(185, 245, 255))

    draw.text((40, 35), "AQUARIUM GREYBOX - 1 UNIT = 1 METER", fill=(235, 242, 248))
    draw.text((40, 70), "Simple single-floor loop / modular walls / separate event glass", fill=(145, 170, 185))
    OUTPUT_PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    image.save(OUTPUT_PREVIEW)


def create_isometric_preview() -> None:
    """Render the generated triangles with a small orthographic painter."""

    width, height = 1600, 1050
    pixels = np.zeros((height, width, 4), dtype=np.uint8)
    pixels[:, :] = (9, 13, 19, 255)
    depth_buffer = np.full((height, width), np.inf, dtype=np.float32)
    camera = (44.0, 38.0, -52.0)
    target = (0.0, 1.5, 0.0)

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
    right = normalize(cross(forward, (0.0, 1.0, 0.0)))
    screen_up = normalize(cross(right, forward))
    light = normalize((-0.35, 0.85, -0.4))
    scale = 17.0

    def project(position):
        relative = subtract(position, target)
        return (
            width * 0.5 + dot(relative, right) * scale,
            height * 0.57 - dot(relative, screen_up) * scale,
            dot(subtract(position, camera), forward),
        )

    triangles = []
    for group_name, group in groups.items():
        rgba = GROUP_COLORS[group_name]
        base_color = tuple(round(channel * 255) for channel in rgba[:3])
        alpha = round(rgba[3] * 255)
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
            projected = [project(vertex) for vertex in vertices]
            brightness = 0.38 + max(dot(normal, light), 0.0) * 0.62
            color = tuple(min(255, round(channel * brightness)) for channel in base_color)
            depth = sum(point[2] for point in projected) / 3.0
            triangles.append(
                (
                    depth,
                    projected,
                    (*color, alpha),
                    group_name,
                )
            )

    def rasterize(triangle, transparent: bool) -> None:
        _, polygon, color, _ = triangle
        points = [projected for projected in polygon]
        x0, y0, z0 = points[0]
        x1, y1, z1 = points[1]
        x2, y2, z2 = points[2]
        min_x = max(0, int(math.floor(min(x0, x1, x2))))
        max_x = min(width - 1, int(math.ceil(max(x0, x1, x2))))
        min_y = max(0, int(math.floor(min(y0, y1, y2))))
        max_y = min(height - 1, int(math.ceil(max(y0, y1, y2))))
        if min_x > max_x or min_y > max_y:
            return
        denominator = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(denominator) < 1e-6:
            return
        yy, xx = np.mgrid[min_y : max_y + 1, min_x : max_x + 1]
        weight0 = ((y1 - y2) * (xx - x2) + (x2 - x1) * (yy - y2)) / denominator
        weight1 = ((y2 - y0) * (xx - x2) + (x0 - x2) * (yy - y2)) / denominator
        weight2 = 1.0 - weight0 - weight1
        inside = (weight0 >= -1e-5) & (weight1 >= -1e-5) & (weight2 >= -1e-5)
        triangle_depth = weight0 * z0 + weight1 * z1 + weight2 * z2
        local_depth = depth_buffer[min_y : max_y + 1, min_x : max_x + 1]
        visible = inside & (triangle_depth <= local_depth + 1e-4)
        if not np.any(visible):
            return
        target = pixels[min_y : max_y + 1, min_x : max_x + 1]
        source_rgb = np.array(color[:3], dtype=np.float32)
        if transparent:
            alpha = color[3] / 255.0
            target_rgb = target[:, :, :3].astype(np.float32)
            target[:, :, :3][visible] = (
                target_rgb[visible] * (1.0 - alpha) + source_rgb * alpha
            ).astype(np.uint8)
        else:
            target[visible] = color
            local_depth[visible] = triangle_depth[visible]

    for triangle in triangles:
        if triangle[3] != "Glass":
            rasterize(triangle, False)
    for triangle in sorted(triangles, key=lambda item: item[0], reverse=True):
        if triangle[3] == "Glass":
            rasterize(triangle, True)

    image = Image.fromarray(pixels, "RGBA")
    draw = ImageDraw.Draw(image, "RGBA")

    draw.rounded_rectangle((28, 26, 650, 108), radius=14, fill=(5, 9, 14, 220))
    draw.text((50, 45), "AQUARIUM GREYBOX / GENERATED GLB", fill=(235, 244, 248, 255))
    draw.text(
        (50, 76),
        "30m main tank - public loop - maintenance and pump rooms",
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
    create_preview()
    create_isometric_preview()
    validate_glb()
    print(
        json.dumps(
            {
                "glb": str(OUTPUT_GLB),
                "plan": str(OUTPUT_PREVIEW),
                "isometric": str(OUTPUT_ISOMETRIC),
                **statistics,
            }
        )
    )
