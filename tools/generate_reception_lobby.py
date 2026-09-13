"""Generate the key-9 closed-aquarium reception lobby.

The render mesh and collision header are emitted from the same dimensions so
the service door cannot drift away from its surrounding wall.  Authored units
are metres.  StageModel mirrors glTF Z and applies the runtime -2.25 m floor
offset.
"""

from __future__ import annotations

import json
import math
from dataclasses import dataclass
from pathlib import Path

import generate_route_01_02 as route


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_GLB = ROOT / "asset" / "model" / "aquarium_reception_lobby.glb"
OUTPUT_HEADER = ROOT / "src" / "generated" / "ReceptionLobbyGenerated.h"
OUTPUT_MANIFEST = ROOT / "artifacts" / "reception_lobby_manifest.json"

FLOOR_OFFSET = -2.25
ROOM_WIDTH = 17.0
ROOM_DEPTH = 8.5
ROOM_HEIGHT = 4.5
RAMP_WIDTH = 4.20
RAMP_RISE = 5.50
RAMP_CLEAR_HEIGHT = 4.00
RAMP_ENTRANCE_WIDTH = 4.80
RAMP_OUTER_EAST = 15.85
RAMP_OUTER_FAR_Z = -23.55
UPPER_FLOOR_Y = 5.50
UPPER_DEPTH = 17.00
UPPER_ARM_WIDTH = 4.80
# Keep both long arms outside the 9.2 m hero-tank frame.  The previous
# centre=9.0 layout overlapped two metres of each deck with the tank volume,
# which was dimensionally connected but visually impossible.
UPPER_ARM_CENTER_X = 11.85
# A real H/U gallery uses a narrow tank-facing bridge and leaves the 1F hall
# open below.  Runtime Z becomes 3.55..7.35, leaving a 0.65 m reveal before
# the acrylic at Z=8.0 instead of burying the guard in it.
UPPER_GALLERY_DEPTH = 6.45
UPPER_CROSS_Z = -4.125
HERO_TANK_HEIGHT = 10.20
HERO_TANK_SURFACE_Y = 10.20
HERO_HALL_CEILING_Y = 11.50


@dataclass(frozen=True)
class Box:
    name: str
    minimum: tuple[float, float, float]
    maximum: tuple[float, float, float]
    tag: str


@dataclass(frozen=True)
class PathSpec:
    name: str
    points: tuple[tuple[float, float, float], ...]
    half_width: float


boxes: list[Box] = []
floor_rects: list[dict] = []
paths: list[PathSpec] = []
features: list[dict] = []


def ensure_material(name: str, rgba: tuple[float, float, float, float]) -> None:
    if name not in route.MATERIALS:
        route.MATERIALS[name] = rgba
        route.groups[name] = route.MeshGroup(name=name, material=name)


def reset() -> None:
    for group in route.groups.values():
        group.positions.clear()
        group.normals.clear()
        group.texcoords.clear()
        group.indices.clear()
    boxes.clear()
    floor_rects.clear()
    paths.clear()
    features.clear()


def runtime_point(point: tuple[float, float, float]) -> tuple[float, float, float]:
    return point[0], point[1] + FLOOR_OFFSET, -point[2]


def add_box(material: str, name: str, center, size,
            collider: str | None = None) -> None:
    # V4 owns these boundaries as split walls with real openings. Do not leave
    # the former closed portal or the horseshoe exit facade inside the route.
    if name in {"Upper_RightOuterWall", "Upper_LeftOuterWall",
                "Upper_LeftOuterWallFront", "Upper_NorthWall",
                "HeroHall_UpperExitLowerSeal", "HeroHall_UpperExitHeader",
                "HeroRamp_UpperPortalMarker", "HeroHall_Ceiling",
                "Upper_CrossSouthParapet", "Upper_CrossSouthParapet_Cap"} or name.startswith("UpperPortal_"):
        return
    if name.startswith("RampMid_"):
        # Fit the display below the curved shoulder of the level corridor.
        center = (center[0] + 4.5, center[1] - 1.6, center[2])
        if name == "RampMid_DisplayWater": material = "TankWaterDisplayBox"
        if name == "RampMid_DisplayGlass": material = "TankGlassReception"
    if name in {"Blackout_Ceiling", "ArchConnector_Ceiling"}:
        center = (center[0], 4.12, center[2])
    if name == "HeroGallery_Ceiling":
        center = (center[0], 5.10, center[2])
    if name in {"HeroHall_WestWallFront", "HeroHall_WestWallRear",
                "HeroHall_EastWallFront", "HeroHall_EastWallRear",
                "HeroHall_SouthWingLeft", "HeroHall_SouthWingRight"}:
        center = (center[0], 2.65, center[2])
        size = (size[0], 5.30, size[2])
    if name == "HeroHall_RampDoorHeader":
        center = (center[0], 4.65, center[2])
        size = (size[0], 1.30, size[2])
    if name == "HeroHall_SouthUpperWall":
        center = (center[0], 4.775, center[2])
        size = (size[0], 1.05, size[2])
    if name == "HeroGallery_EntranceUpperSeal":
        center = (center[0], 5.20, center[2])
        size = (size[0], .20, size[2])
    if name in {"Blackout_WestWall", "Blackout_EastWall",
                "ArchConnector_WestWall", "ArchConnector_EastWall"}:
        center = (center[0], 2.0, center[2])
        size = (size[0], 4.0, size[2])
    if name in {"ArchPortal_Header", "ArchConnector_EntranceHeader"}:
        center = (center[0], 3.75, center[2])
        size = (size[0], 0.5, size[2])
    route.add_box(material, name, center, size)
    if collider:
        cx, cy, cz = runtime_point(center)
        sx, sy, sz = size
        boxes.append(Box(
            name,
            (cx - sx * 0.5, cy - sy * 0.5, cz - sz * 0.5),
            (cx + sx * 0.5, cy + sy * 0.5, cz + sz * 0.5),
            collider))


def add_collider_box(name: str, center, size, tag: str = "Solid") -> None:
    """Add collision without duplicating visible geometry."""
    cx, cy, cz = runtime_point(center)
    sx, sy, sz = size
    boxes.append(Box(
        name,
        (cx - sx * 0.5, cy - sy * 0.5, cz - sz * 0.5),
        (cx + sx * 0.5, cy + sy * 0.5, cz + sz * 0.5),
        tag))


def add_floor(name: str, center_x: float, center_z: float,
              width: float, depth: float, authored_y: float = 0.0,
              material: str = "ReceptionFloor") -> None:
    """Emit one visible slab and its exactly matching runtime walk rectangle."""
    add_box(material, name, (center_x, authored_y - 0.09, center_z),
            (width, 0.18, depth), None)
    floor_rects.append({
        "name": name,
        "minimum_x": center_x - width * 0.5,
        "maximum_x": center_x + width * 0.5,
        "minimum_z": -(center_z + depth * 0.5),
        "maximum_z": -(center_z - depth * 0.5),
        "floor_y": authored_y + FLOOR_OFFSET,
    })


def add_runtime_path(name: str, authored_points, half_width: float) -> None:
    paths.append(PathSpec(
        name,
        tuple(runtime_point(point) for point in authored_points),
        half_width))


def build_compact_ramp_points():
    from upper_floor_v4 import ramp_points
    return ramp_points()


def build_legacy_ramp_points() -> tuple[tuple[float, float, float], ...]:
    """Return the reviewed ramp as one frozen, shared centreline.

    No independent radius, length or rise formula remains here.  The visible
    floor, arched shell and collision path all consume these same points.
    """
    return (
        (10.5000, 0.0000, -3.0000), (11.0000, 0.0000, -3.0000),
        (11.3911, 0.0000, -3.0308), (11.7725, 0.0080, -3.1224),
        (12.1350, 0.0450, -3.2725), (12.4695, 0.0820, -3.4775),
        (12.7678, 0.1189, -3.7322), (13.0225, 0.1559, -4.0305),
        (13.2275, 0.1929, -4.3650), (13.3776, 0.2299, -4.7275),
        (13.4692, 0.2669, -5.1089), (13.5000, 0.3039, -5.5000),
        (13.5000, 0.5467, -8.0750), (13.5000, 0.7895, -10.6500),
        (13.5000, 1.0323, -13.2250), (13.5000, 1.2751, -15.8000),
        (13.4151, 1.3348, -16.4270), (13.1615, 1.3979, -17.0461),
        (12.7424, 1.4671, -17.6496), (12.1631, 1.5444, -18.2297),
        (11.4308, 1.6308, -18.7794), (10.5547, 1.7265, -19.2915),
        (9.5459, 1.8313, -19.7598), (8.4171, 1.9449, -20.1783),
        (7.1824, 2.0662, -20.5417), (5.8574, 2.1944, -20.8454),
        (4.4588, 2.3282, -21.0857), (3.0040, 2.4664, -21.2596),
        (1.5115, 2.6074, -21.3648), (0.0000, 2.7500, -21.4000),
        (-1.5115, 2.8926, -21.3648), (-3.0040, 3.0336, -21.2596),
        (-4.4588, 3.1718, -21.0857), (-5.8574, 3.3056, -20.8454),
        (-7.1824, 3.4338, -20.5417), (-8.4171, 3.5551, -20.1783),
        (-9.5459, 3.6687, -19.7598), (-10.5547, 3.7735, -19.2915),
        (-11.4308, 3.8692, -18.7794), (-12.1631, 3.9556, -18.2297),
        (-12.7424, 4.0329, -17.6496), (-13.1615, 4.1021, -17.0461),
        (-13.4151, 4.1652, -16.4270), (-13.5000, 4.2249, -15.8000),
        (-13.5000, 4.4606, -13.3000), (-13.5000, 4.6964, -10.8000),
        (-13.5000, 4.9321, -8.3000), (-13.5000, 5.1678, -5.8000),
        (-13.4692, 5.2048, -5.4089), (-13.3776, 5.2418, -5.0275),
        (-13.2275, 5.2788, -4.6650), (-13.0225, 5.3158, -4.3305),
        (-12.7678, 5.3528, -4.0322), (-12.4695, 5.3898, -3.7775),
        (-12.1350, 5.4267, -3.5725), (-11.7725, 5.4637, -3.4224),
        (-11.3911, 5.5000, -3.3308), (-11.0000, 5.5000, -3.3000),
        (-10.6000, 5.5000, -3.3000), (-10.2000, 5.5000, -3.3000),
    )


def add_path_strip(material: str, name: str, points, width: float) -> None:
    """Build one watertight mitered slab from the collision centre line."""
    collision_points = points
    if name == "HeroRamp_Main":
        # Collision overlaps the hall for a stable hand-off, but the visible
        # floor starts exactly at the hall edge to prevent coplanar flicker.
        points = ((11.5, points[0][1], points[0][2]), *points[1:])
    thickness = 0.16
    half_width = width * 0.5
    rings = []

    def tangent(first, second):
        dx, dz = second[0] - first[0], second[2] - first[2]
        length = max(math.hypot(dx, dz), 0.0001)
        return dx / length, dz / length

    for index, (x, y, z) in enumerate(points):
        if index == 0:
            previous_tangent = tangent(points[0], points[1])
        else:
            previous_tangent = tangent(points[index - 1], points[index])
        if index == len(points) - 1:
            next_tangent = previous_tangent
        else:
            next_tangent = tangent(points[index], points[index + 1])
        previous_normal = (-previous_tangent[1], previous_tangent[0])
        next_normal = (-next_tangent[1], next_tangent[0])
        miter_x = previous_normal[0] + next_normal[0]
        miter_z = previous_normal[1] + next_normal[1]
        miter_length = math.hypot(miter_x, miter_z)
        if miter_length < 0.001:
            miter_x, miter_z = next_normal
        else:
            miter_x, miter_z = miter_x / miter_length, miter_z / miter_length
        denominator = max(abs(miter_x * next_normal[0] +
                              miter_z * next_normal[1]), 0.30)
        offset = min(half_width / denominator, half_width * 1.75)
        rings.append(((x + miter_x * offset, y, z + miter_z * offset),
                      (x - miter_x * offset, y, z - miter_z * offset)))

    for index in range(len(rings) - 1):
        left0, right0 = rings[index]
        left1, right1 = rings[index + 1]
        bottom_left0 = (left0[0], left0[1] - thickness, left0[2])
        bottom_left1 = (left1[0], left1[1] - thickness, left1[2])
        bottom_right1 = (right1[0], right1[1] - thickness, right1[2])
        bottom_right0 = (right0[0], right0[1] - thickness, right0[2])
        append_quad(material, (left0, left1, right1, right0),
                    (0.0, 1.0, 0.0))
        append_quad(material,
                    (bottom_right0, bottom_right1,
                     bottom_left1, bottom_left0),
                    (0.0, -1.0, 0.0))
        append_quad(material,
                    (bottom_left0, bottom_left1, left1, left0),
                    (-1.0, 0.0, 0.0))
        append_quad(material,
                    (right0, right1, bottom_right1, bottom_right0),
                    (1.0, 0.0, 0.0))
    add_runtime_path(name, collision_points, width * 0.5)


def add_path_arch_shell(material: str, name: str, points, width: float,
                        clear_height: float = 3.0,
                        spring_height: float = 1.45,
                        arch_segments: int = 8) -> None:
    """Build a continuous arched tunnel from the walk-path centre line.

    Each cross-section uses the averaged horizontal tangent of its neighbours.
    The resulting mitered rings share vertices along corners and landings, so
    the ceiling cannot open triangular cracks when the path turns.
    """
    del name
    half_width = width * 0.5 + 0.10
    rings = []
    for index, (x, y, z) in enumerate(points):
        previous = points[max(index - 1, 0)]
        following = points[min(index + 1, len(points) - 1)]
        tangent_x = following[0] - previous[0]
        tangent_z = following[2] - previous[2]
        tangent_length = max(math.hypot(tangent_x, tangent_z), 0.0001)
        right_x = -tangent_z / tangent_length
        right_z = tangent_x / tangent_length
        ring = [
            (x - right_x * half_width, y, z - right_z * half_width),
            (x - right_x * half_width, y + spring_height,
             z - right_z * half_width),
        ]
        for arch_index in range(1, arch_segments):
            angle = math.pi - math.pi * arch_index / arch_segments
            offset = math.cos(angle) * half_width
            height = spring_height + math.sin(angle) * (
                clear_height - spring_height)
            ring.append((x + right_x * offset, y + height,
                         z + right_z * offset))
        ring.extend((
            (x + right_x * half_width, y + spring_height,
             z + right_z * half_width),
            (x + right_x * half_width, y, z + right_z * half_width),
        ))
        rings.append(ring)

    for ring_index in range(len(rings) - 1):
        first_ring = rings[ring_index]
        second_ring = rings[ring_index + 1]
        for cross_index in range(len(first_ring) - 1):
            vertices = (first_ring[cross_index],
                        first_ring[cross_index + 1],
                        second_ring[cross_index + 1],
                        second_ring[cross_index])
            # One physical surface, visible from either side with CULL_NONE.
            append_quad(material, vertices, (0.0, -1.0, 0.0))
            # StageModel uses CULL_NONE: a reversed coplanar copy competes in
            # depth and lighting. One face already renders from both sides.


def append_quad(material: str, vertices, normal) -> None:
    group = route.groups[material]
    base = len(group.positions) // 3
    for vertex, uv in zip(vertices, ((0, 0), (0, 1), (1, 1), (1, 0))):
        group.positions.extend(vertex)
        group.normals.extend(normal)
        group.texcoords.extend(uv)
    group.indices.extend((base, base + 1, base + 2,
                          base, base + 2, base + 3))


def add_arch_portal_band(material: str, x: float, center_z: float,
                         clear_width: float, clear_height: float,
                         spring_height: float, band_width: float,
                         floor_y: float = 0.0) -> None:
    """Add a double-sided, front-facing architrave around one clear opening.

    This is intentionally independent from the tunnel shell.  The shell owns
    the long interior surface; this thin band gives the public hall a clean,
    deliberate portal silhouette without another transparent or lit layer.
    """
    if x < 0:
        return  # The upper exit is now in the north facade, not the west wall.
    segments = 12
    inner_half = clear_width * 0.5
    outer_half = inner_half + band_width
    inner = [(center_z - inner_half, floor_y)]
    outer = [(center_z - outer_half, floor_y)]
    for index in range(segments + 1):
        angle = math.pi - math.pi * index / segments
        inner.append((center_z + math.cos(angle) * inner_half,
                      floor_y + spring_height + math.sin(angle) *
                      (clear_height - spring_height)))
        outer.append((center_z + math.cos(angle) * outer_half,
                      floor_y + spring_height + math.sin(angle) *
                      (clear_height + band_width - spring_height)))
    inner.append((center_z + inner_half, floor_y))
    outer.append((center_z + outer_half, floor_y))
    for index in range(len(inner) - 1):
        vertices = (
            (x, outer[index][1], outer[index][0]),
            (x, outer[index + 1][1], outer[index + 1][0]),
            (x, inner[index + 1][1], inner[index + 1][0]),
            (x, inner[index][1], inner[index][0]),
        )
        append_quad(material, vertices, (-1.0, 0.0, 0.0))


def append_horizontal_fan(material: str, polygon, y: float,
                          upward: bool) -> None:
    group = route.groups[material]
    center = (sum(point[0] for point in polygon) / len(polygon),
              sum(point[1] for point in polygon) / len(polygon))
    normal = (0.0, 1.0 if upward else -1.0, 0.0)
    for index, first in enumerate(polygon):
        second = polygon[(index + 1) % len(polygon)]
        base = len(group.positions) // 3
        vertices = ((center[0], y, center[1]),
                    (first[0], y, first[1]),
                    (second[0], y, second[1]))
        if not upward:
            vertices = (vertices[0], vertices[2], vertices[1])
        for vertex, uv in zip(vertices, ((0.5, 0.5), (0.0, 0.0), (1.0, 0.0))):
            group.positions.extend(vertex)
            group.normals.extend(normal)
            group.texcoords.extend(uv)
        group.indices.extend((base, base + 1, base + 2))


def add_extruded_polygon(material: str, polygon, y0: float, y1: float) -> None:
    """Append a convex vertical prism with a clean top and faceted sides."""
    for index, first in enumerate(polygon):
        second = polygon[(index + 1) % len(polygon)]
        dx, dz = second[0] - first[0], second[1] - first[1]
        length = max(math.hypot(dx, dz), 0.0001)
        append_quad(material,
                    ((first[0], y0, first[1]), (first[0], y1, first[1]),
                     (second[0], y1, second[1]), (second[0], y0, second[1])),
                    (dz / length, 0.0, -dx / length))
    group = route.groups[material]
    center = (sum(point[0] for point in polygon) / len(polygon),
              sum(point[1] for point in polygon) / len(polygon))
    for index, first in enumerate(polygon):
        second = polygon[(index + 1) % len(polygon)]
        base = len(group.positions) // 3
        for vertex, uv in (
            ((center[0], y1, center[1]), (0.5, 0.5)),
            ((first[0], y1, first[1]), (0.0, 0.0)),
            ((second[0], y1, second[1]), (1.0, 0.0)),
        ):
            group.positions.extend(vertex)
            group.normals.extend((0.0, 1.0, 0.0))
            group.texcoords.extend(uv)
        group.indices.extend((base, base + 1, base + 2))


def add_ticket_desk(index: int, x: float) -> None:
    """Rounded-front staffed counter matching the approved entrance image."""
    z = 1.18
    polygon = [
        (x - 0.53, z - 0.48), (x + 0.53, z - 0.48),
        (x + 0.58, z + 0.25), (x + 0.43, z + 0.48),
        (x - 0.43, z + 0.48), (x - 0.58, z + 0.25),
    ]
    add_extruded_polygon("ReceptionMetal", polygon, 0.0, 0.13)
    add_extruded_polygon("ReceptionCounter", polygon, 0.13, 1.04)
    top = [(px * 1.025 - x * 0.025, pz * 1.025 - z * 0.025)
           for px, pz in polygon]
    add_extruded_polygon("ReceptionMetal", top, 1.04, 1.12)
    add_collider_box(f"Reception_TicketDesk_{index}",
                     (x, 0.56, z), (1.16, 1.12, 0.96))


def add_curved_reception_counter() -> None:
    """Quarter-ellipse counter enclosing the north-east staff corner."""
    y0, body_top, top = 0.0, 1.02, 1.14
    segments = 24
    cx, cz = 8.18, 0.36
    front_rx, front_rz = 4.18, 3.04
    back_rx, back_rz = 3.42, 2.30
    front = [
        (cx - front_rx * math.cos(math.pi * 0.5 * index / segments),
         cz + front_rz * math.sin(math.pi * 0.5 * index / segments))
        for index in range(segments + 1)
    ]
    back = [
        (cx - back_rx * math.cos(math.pi * 0.5 * index / segments),
         cz + back_rz * math.sin(math.pi * 0.5 * index / segments))
        for index in range(segments + 1)
    ]
    for index in range(segments):
        f0, f1 = front[index], front[index + 1]
        b0, b1 = back[index], back[index + 1]
        dx, dz = f1[0] - f0[0], f1[1] - f0[1]
        inv = 1.0 / max(math.hypot(dx, dz), 0.0001)
        append_quad("ReceptionCounter",
                    ((f0[0], y0, f0[1]), (f0[0], body_top, f0[1]),
                     (f1[0], body_top, f1[1]), (f1[0], y0, f1[1])),
                    (-dz * inv, 0.0, dx * inv))
        append_quad("ReceptionCounter",
                    ((b1[0], y0, b1[1]), (b1[0], body_top, b1[1]),
                     (b0[0], body_top, b0[1]), (b0[0], y0, b0[1])),
                    (dz * inv, 0.0, -dx * inv))
        # A substantial metal-edged transaction slab reproduces the upper
        # counter instead of leaving a paper-thin single surface.
        append_quad("ReceptionMetal",
                    ((f0[0], body_top, f0[1]), (f0[0], top, f0[1]),
                     (f1[0], top, f1[1]), (f1[0], body_top, f1[1])),
                    (-dz * inv, 0.0, dx * inv))
        append_quad("ReceptionMetal",
                    ((b1[0], body_top, b1[1]), (b1[0], top, b1[1]),
                     (b0[0], top, b0[1]), (b0[0], body_top, b0[1])),
                    (dz * inv, 0.0, -dx * inv))
        append_quad("ReceptionMetal",
                    ((b0[0], top, b0[1]), (f0[0], top, f0[1]),
                     (f1[0], top, f1[1]), (b1[0], top, b1[1])),
                    (0.0, 1.0, 0.0))
    append_quad("ReceptionCounter",
                ((back[0][0], y0, back[0][1]),
                 (back[0][0], top, back[0][1]),
                 (front[0][0], top, front[0][1]),
                 (front[0][0], y0, front[0][1])),
                (-1.0, 0.0, 0.0))
    append_quad("ReceptionCounter",
                ((front[-1][0], y0, front[-1][1]),
                 (front[-1][0], top, front[-1][1]),
                 (back[-1][0], top, back[-1][1]),
                 (back[-1][0], y0, back[-1][1])),
                (1.0, 0.0, 0.0))

    # Architectural returns bury both ends into the north/east wall finishes.
    # Without these short pieces the counter reads as freestanding from oblique
    # angles, even though its curve is aimed at the corner.
    north_return_depth = front[0][1] - 0.10
    add_box("ReceptionCounter", "ReceptionCounter_NorthReturnBody",
            ((front[0][0] + back[0][0]) * 0.5, body_top * 0.5,
             0.10 + north_return_depth * 0.5),
            (back[0][0] - front[0][0], body_top, north_return_depth), "Solid")
    add_box("ReceptionMetal", "ReceptionCounter_NorthReturnTop",
            ((front[0][0] + back[0][0]) * 0.5, (body_top + top) * 0.5,
             0.10 + north_return_depth * 0.5),
            (back[0][0] - front[0][0], top - body_top,
             north_return_depth), None)

    east_return_width = 8.40 - front[-1][0]
    add_box("ReceptionCounter", "ReceptionCounter_EastReturnBody",
            (front[-1][0] + east_return_width * 0.5, body_top * 0.5,
             (back[-1][1] + front[-1][1]) * 0.5),
            (east_return_width, body_top,
             front[-1][1] - back[-1][1]), "Solid")
    add_box("ReceptionMetal", "ReceptionCounter_EastReturnTop",
            (front[-1][0] + east_return_width * 0.5,
             (body_top + top) * 0.5,
             (back[-1][1] + front[-1][1]) * 0.5),
            (east_return_width, top - body_top,
             front[-1][1] - back[-1][1]), None)
    # Six small AABBs follow the arc closely enough for a capsule while the
    # visible counter stays smoothly curved.
    collider_segments = 6
    stride = segments // collider_segments
    for collider_index in range(collider_segments):
        first = collider_index * stride
        last = segments if collider_index == collider_segments - 1 else first + stride
        points = front[first:last + 1] + back[first:last + 1]
        minimum_x = min(point[0] for point in points)
        maximum_x = max(point[0] for point in points)
        minimum_z = min(point[1] for point in points)
        maximum_z = max(point[1] for point in points)
        add_collider_box(
            f"ReceptionCounter_Collision_{collider_index + 1}",
            ((minimum_x + maximum_x) * 0.5, top * 0.5,
             (minimum_z + maximum_z) * 0.5),
            (maximum_x - minimum_x, top, maximum_z - minimum_z))


def add_curved_counter_soffit() -> None:
    """Quarter-ellipse lowered ceiling covering the staff corner."""
    segments = 24
    cx, cz = 8.45, 0.18
    radius_x, radius_z = 4.72, 3.50
    arc = [
        (cx - radius_x * math.cos(math.pi * 0.5 * index / segments),
         cz + radius_z * math.sin(math.pi * 0.5 * index / segments))
        for index in range(segments + 1)
    ]
    polygon = [arc[0], (cx, cz), arc[-1], *reversed(arc[1:-1])]
    y0, y1 = 4.02, 4.28
    add_extruded_polygon("ReceptionWall", polygon, y0, y1)
    append_horizontal_fan("ReceptionWall", polygon, y0, False)


def add_curved_reception_screen() -> None:
    """Acrylic security screen following the public face of the counter."""
    segments = 24
    cx, cz = 8.18, 0.36
    radius_x, radius_z = 4.18, 3.04
    arc = [
        (cx - radius_x * math.cos(math.pi * 0.5 * index / segments),
         cz + radius_z * math.sin(math.pi * 0.5 * index / segments))
        for index in range(segments + 1)
    ]
    acrylic_bottom, acrylic_top, soffit_bottom = 1.14, 3.48, 4.02
    for first, second in zip(arc, arc[1:]):
        dx, dz = second[0] - first[0], second[1] - first[1]
        inv = 1.0 / max(math.hypot(dx, dz), 0.0001)
        normal = (-dz * inv, 0.0, dx * inv)
        append_quad("TankGlassReception",
                    ((first[0], acrylic_bottom, first[1]),
                     (first[0], acrylic_top, first[1]),
                     (second[0], acrylic_top, second[1]),
                     (second[0], acrylic_bottom, second[1])), normal)
        append_quad("ReceptionWall",
                    ((first[0], acrylic_top, first[1]),
                     (first[0], soffit_bottom, first[1]),
                     (second[0], soffit_bottom, second[1]),
                     (second[0], acrylic_top, second[1])), normal)

    # Eight slightly overlapping AABBs follow the curve closely enough to
    # block both the player capsule and interaction rays from the public side.
    for collider_index in range(8):
        first = arc[collider_index * 3]
        last = arc[(collider_index + 1) * 3]
        minimum_x, maximum_x = sorted((first[0], last[0]))
        minimum_z, maximum_z = sorted((first[1], last[1]))
        add_collider_box(
            f"Reception_AcrylicScreen_{collider_index + 1}",
            ((minimum_x + maximum_x) * .5,
             (acrylic_bottom + soffit_bottom) * .5,
             (minimum_z + maximum_z) * .5),
            (maximum_x - minimum_x + .10,
             soffit_bottom - acrylic_bottom,
             maximum_z - minimum_z + .10), "Glass")


def add_wall_x(name: str, x0: float, x1: float, z: float,
               y0: float = 0.0, height: float = ROOM_HEIGHT) -> None:
    if x1 <= x0:
        return
    add_box("ReceptionWall", name,
            ((x0 + x1) * 0.5, y0 + height * 0.5, z),
            (x1 - x0, height, 0.26), "Solid")


def add_wall_z(name: str, x: float, z0: float, z1: float) -> None:
    add_box("ReceptionWall", name,
            (x, ROOM_HEIGHT * 0.5, (z0 + z1) * 0.5),
            (0.26, ROOM_HEIGHT, z1 - z0), "Solid")


def add_faceted_rock(center, size, seed: float, segments: int = 12) -> None:
    """Low-poly reef mass: irregular enough to read naturally, one draw batch."""
    cx, cy, cz = center
    rx, ry, rz = size
    lower, upper = [], []
    for index in range(segments):
        angle = math.tau * index / segments
        variation = 0.82 + 0.16 * math.sin(seed * 1.73 + index * 2.41)
        lower.append((cx + math.cos(angle) * rx * variation,
                      cy - ry * 0.30,
                      cz + math.sin(angle) * rz * variation))
        upper.append((cx + math.cos(angle + 0.18) * rx * variation * 0.72,
                      cy + ry * 0.32,
                      cz + math.sin(angle + 0.18) * rz * variation * 0.72))
    bottom = (cx, cy - ry * 0.58, cz)
    top = (cx + rx * 0.08, cy + ry * 0.49, cz - rz * 0.06)
    group = route.groups["WatatsumiRock"]

    def triangle(vertices) -> None:
        ax, ay, az = vertices[0]
        bx, by, bz = vertices[1]
        cx2, cy2, cz2 = vertices[2]
        ux, uy, uz = bx - ax, by - ay, bz - az
        vx, vy, vz = cx2 - ax, cy2 - ay, cz2 - az
        nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
        inv = 1.0 / max(math.sqrt(nx * nx + ny * ny + nz * nz), 0.0001)
        base = len(group.positions) // 3
        for vertex, uv in zip(vertices, ((0, 0), (0.5, 1), (1, 0))):
            group.positions.extend(vertex)
            group.normals.extend((nx * inv, ny * inv, nz * inv))
            group.texcoords.extend(uv)
        group.indices.extend((base, base + 1, base + 2))

    for index in range(segments):
        nxt = (index + 1) % segments
        triangle((bottom, lower[nxt], lower[index]))
        triangle((lower[index], lower[nxt], upper[nxt]))
        triangle((lower[index], upper[nxt], upper[index]))
        triangle((upper[index], upper[nxt], top))


def add_bubble_ellipsoid(center, radii, seed: float,
                         latitude_segments: int = 3,
                         longitude_segments: int = 6) -> None:
    """Append one smooth bubble to the shared transparent batch.

    UV.x is constant over the ellipsoid. The stage vertex shader uses that
    seed to move the complete bubble without a particle buffer or CPU update.
    """
    group = route.groups["WatatsumiBubble"]
    cx, cy, cz = center
    rx, ry, rz = radii
    base = len(group.positions) // 3
    seed_uv = seed - math.floor(seed)
    for latitude in range(latitude_segments + 1):
        polar = math.pi * latitude / latitude_segments
        sin_polar, cos_polar = math.sin(polar), math.cos(polar)
        for longitude in range(longitude_segments + 1):
            azimuth = math.tau * longitude / longitude_segments
            nx = math.cos(azimuth) * sin_polar
            ny = cos_polar
            nz = math.sin(azimuth) * sin_polar
            group.positions.extend((cx + nx * rx, cy + ny * ry, cz + nz * rz))
            ex, ey, ez = nx / rx, ny / ry, nz / rz
            inverse_length = 1.0 / math.sqrt(ex * ex + ey * ey + ez * ez)
            group.normals.extend((ex * inverse_length,
                                  ey * inverse_length,
                                  ez * inverse_length))
            group.texcoords.extend((seed_uv, 0.0))
    row = longitude_segments + 1
    for latitude in range(latitude_segments):
        for longitude in range(longitude_segments):
            a = base + latitude * row + longitude
            b = a + row
            group.indices.extend((a, b, a + 1, a + 1, b, b + 1))


def add_hero_tank_bubbles() -> None:
    """Three sparse aeration plumes with real size and speed variation."""
    emitters = ((-5.55, -14.45), (0.35, -15.05), (5.65, -14.20))
    bubble_count = 32
    for emitter_index, (base_x, base_z) in enumerate(emitters):
        for index in range(bubble_count):
            phase = emitter_index * 11.73 + index * 2.399963
            seed = (math.sin(phase * 1.917) * 43758.5453) % 1.0
            x = base_x + math.sin(phase) * (0.07 + seed * 0.09)
            z = base_z + math.cos(phase * 1.31) * (0.06 + seed * 0.08)
            size_selector = index % 10
            if size_selector == 0:
                radius = 0.060 + seed * 0.018
            elif size_selector < 3:
                radius = 0.040 + seed * 0.012
            else:
                radius = 0.020 + seed * 0.010
            add_bubble_ellipsoid(
                (x, 0.42, z),
                (radius * (0.88 + seed * 0.10),
                 radius * (1.12 + seed * 0.34),
                 radius),
                seed)
    features.append({"type": "hero_tank_bubbles", "emitters": len(emitters),
                     "count": len(emitters) * bubble_count,
                     "draw_batches": 1})


def add_bench(name: str, x: float, z: float, yaw90: bool = False) -> None:
    seat_size = (2.35, 0.12, 0.62) if not yaw90 else (0.62, 0.12, 2.35)
    add_box("ReceptionCounter", f"{name}_Seat", (x, 0.48, z), seat_size, "Solid")
    for sx, sz in ((-0.92, 0.0), (0.92, 0.0)) if not yaw90 else ((0.0, -0.92), (0.0, 0.92)):
        add_box("ReceptionMetal", f"{name}_Leg_{sx}_{sz}",
                (x + sx, 0.23, z + sz), (0.10, 0.46, 0.10), None)


def build_integrated_hero_tank() -> None:
    """Two-storey hero hall with a broad upper observation gallery."""
    # Public hall: gate and tank glass share one datum. Eight metres of clear
    # viewing distance makes the monumental pane readable without dead space.
    add_floor("HeroHall_Floor", 0.0, -4.0, 23.0, 8.0,
              material="WatatsumiRamp")
    add_box("ReceptionWall", "HeroHall_Ceiling",
            (0.0, HERO_HALL_CEILING_Y, -4.0),
            (23.0, 0.30, 8.0), None)
    # The reception is intentionally lower than the tank hall.  Close the
    # entire height change instead of leaving the two ceiling slabs floating.
    # The lower centre remains the approved six-metre gate opening.
    add_box("ReceptionWall", "HeroHall_SouthWingLeft",
            (-10.0, HERO_HALL_CEILING_Y * 0.5, -0.15),
            (3.0, HERO_HALL_CEILING_Y, 0.30), "Solid")
    add_box("ReceptionWall", "HeroHall_SouthWingRight",
            (10.0, HERO_HALL_CEILING_Y * 0.5, -0.15),
            (3.0, HERO_HALL_CEILING_Y, 0.30), "Solid")
    add_box("ReceptionWall", "HeroHall_SouthUpperWall",
            (0.0, 7.875, -0.15), (17.0, 7.25, 0.30), "Solid")
    # The key-6 route returns through the opposite wall on 2F.  Split the west
    # facade around that high opening, but seal its lower storey completely so
    # the side gallery never sees an accidental ground-floor doorway.
    add_box("ReceptionWall", "HeroHall_WestWallFront",
            (-11.50, HERO_HALL_CEILING_Y * 0.5, -0.45),
            (0.30, HERO_HALL_CEILING_Y, 0.90), "Solid")
    add_box("ReceptionWall", "HeroHall_WestWallRear",
            (-11.50, HERO_HALL_CEILING_Y * 0.5, -6.50),
            (0.30, HERO_HALL_CEILING_Y, 1.60), "Solid")
    # Close the 0.70 m interval between the hero-hall rear wall (runtime
    # Z=7.30) and the side-gallery wall (runtime Z=8.00). This exact omission
    # was visible as a floor-to-ceiling blue slit when facing west.
    add_box("ReceptionWall", "HeroHall_WestGalleryClosure",
            (-11.50, 2.65, -7.65),
            (0.30, 5.30, 0.70), "Solid")
    add_box("ReceptionWall", "HeroHall_UpperExitLowerSeal",
            (-11.50, 2.55, -3.30),
            (0.30, 5.10, RAMP_ENTRANCE_WIDTH), "Solid")
    add_box("ReceptionWall", "HeroHall_UpperExitHeader",
            (-11.50, HERO_HALL_CEILING_Y - 0.075, -3.30),
            (0.30, 0.15, RAMP_ENTRANCE_WIDTH), "Solid")
    # A generous but controlled lower portal sits immediately beside the hero
    # tank.  It is narrower than key 6's 6.2 m opening, scaled to this hall.
    add_box("ReceptionWall", "HeroHall_EastWallFront",
            (11.50, HERO_HALL_CEILING_Y * 0.5, -0.30),
            (0.30, HERO_HALL_CEILING_Y, 0.60), "Solid")
    add_box("ReceptionWall", "HeroHall_EastWallRear",
            (11.50, HERO_HALL_CEILING_Y * 0.5, -6.70),
            (0.30, HERO_HALL_CEILING_Y, 2.60), "Solid")
    add_box("ReceptionWall", "HeroHall_RampDoorHeader",
            (11.50, 7.75, -3.00),
            (0.42, 7.50, RAMP_ENTRANCE_WIDTH), "Solid")
    # Deep jambs create a deliberate portal reveal instead of a paper-thin
    # rectangular cut in the room shell. They sit outside the clear opening.
    add_box("ReceptionWall", "HeroRamp_PortalNearJamb",
            (11.10, 2.00, -0.45), (0.80, 4.00, 0.30), "Solid")
    add_box("ReceptionWall", "HeroRamp_PortalFarJamb",
            (11.10, 2.00, -5.55), (0.80, 4.00, 0.30), "Solid")
    add_arch_portal_band("ReceptionMetal", 10.68, -3.00,
                         RAMP_ENTRANCE_WIDTH, RAMP_CLEAR_HEIGHT,
                         1.55, 0.22)
    add_arch_portal_band("WatatsumiEmitter", 10.64, -3.00,
                         RAMP_ENTRANCE_WIDTH + 0.06,
                         RAMP_CLEAR_HEIGHT + 0.04,
                         1.57, 0.035)
    add_arch_portal_band("ReceptionMetal", -10.68, -3.30,
                         RAMP_ENTRANCE_WIDTH, RAMP_CLEAR_HEIGHT,
                         1.65, 0.22, UPPER_FLOOR_Y)
    add_arch_portal_band("WatatsumiEmitter", -10.64, -3.30,
                         RAMP_ENTRANCE_WIDTH + 0.06,
                         RAMP_CLEAR_HEIGHT + 0.04,
                         1.67, 0.035, UPPER_FLOOR_Y)
    # Restrained crown markers work as wayfinding signs without introducing
    # another texture, font atlas, light pass, or draw batch.
    add_box("WatatsumiEmitter", "HeroRamp_LowerPortalMarker",
            (10.62, 4.42, -3.00), (0.05, 0.07, 1.50), None)
    add_box("WatatsumiEmitter", "HeroRamp_UpperPortalMarker",
            (-10.62, 9.22, -3.30), (0.05, 0.07, 1.50), None)

    # Seventeen-by-10.2 metre hero pane. Water and acrylic are each a single
    # layer to avoid the former full-volume transparent overdraw regression.
    add_box("WatatsumiArchitecture", "HeroTank_FrameLeft",
            (-8.85, HERO_TANK_HEIGHT * 0.5, -8.08),
            (0.70, HERO_TANK_HEIGHT + 0.8, 0.42), "Solid")
    add_box("WatatsumiArchitecture", "HeroTank_FrameRight",
            (8.85, HERO_TANK_HEIGHT * 0.5, -8.08),
            (0.70, HERO_TANK_HEIGHT + 0.8, 0.42), "Solid")
    add_box("WatatsumiArchitecture", "HeroTank_FrameTop",
            (0.0, HERO_TANK_HEIGHT + 0.25, -8.08),
            (18.40, 0.70, 0.42), "Solid")
    add_box("WatatsumiArchitecture", "HeroTank_FrameSill", (0.0, 0.18, -8.08),
            (18.40, 0.36, 0.54), "Solid")
    add_box("WatatsumiWater", "HeroTank_WaterInterface",
            (0.0, HERO_TANK_HEIGHT * 0.5, -8.22),
            (17.0, HERO_TANK_HEIGHT, 0.06), None)
    add_box("WatatsumiGlass", "HeroTank_Acrylic",
            (0.0, HERO_TANK_HEIGHT * 0.5, -8.04),
            (17.0, HERO_TANK_HEIGHT, 0.10), "Glass")
    # The right side is not a public viewing face.  Close it as a proper tank
    # wall so the service void between the tank and switchback ramp can never
    # be seen from the hall.
    add_box("WatatsumiArchitecture", "HeroTank_RightSideWall",
            (8.64, HERO_TANK_HEIGHT * 0.5, -12.0),
            (0.28, HERO_TANK_HEIGHT, 8.0), "Solid")
    # This return seals the 1F service cavity only. Stop it at the underside of
    # the 18 cm upper slab: a top face flush with the walk surface fought the
    # deck depth and appeared as a serrated black rectangle while turning.
    add_box("WatatsumiArchitecture", "HeroHall_NorthEastReturn",
            (10.35, 2.66, -7.92), (2.30, 5.32, 0.30), "Solid")
    # This is an underwater service shell, not a dry room wall.  Sharing the
    # reef receiver material lets the same blue attenuation and moving light
    # reach it; the former ReceptionWall path caught unfiltered white local
    # light and made the whole tank look like a grey box.
    add_box("WatatsumiRock", "HeroTank_BackWall",
            (0.0, HERO_TANK_HEIGHT * 0.5, -16.0),
            (17.0, HERO_TANK_HEIGHT, 0.28), None)
    add_box("WatatsumiArchitecture", "HeroTank_Bed", (0.0, -0.08, -12.0),
            (17.0, 0.18, 8.0), None)
    # A subtly animated surface exists above the visible window but is sealed
    # from the 2F route by architecture, as approved.
    add_box("WatatsumiWaterSurface", "HeroTank_Surface",
            (0.0, HERO_TANK_SURFACE_Y, -12.0),
            (17.0, 0.04, 8.0), None)
    add_hero_tank_bubbles()
    add_box("WatatsumiArchitecture", "HeroTank_SealedCrown",
            (0.0, 10.90, -12.0), (18.4, 1.10, 8.4), None)
    # Illumination is analytic and starts above the sealed crown.  Do not add
    # luminous proxy cards below it: they were visible as three white ceiling
    # rectangles from the upper gallery and added no actual light.

    # Asymmetric side reefs frame a clean central swimming corridor.
    rock_specs = (
        ((-7.10, 0.72, -14.35), (1.45, 1.25, 1.20), 1.0),
        ((-6.15, 1.20, -14.75), (1.55, 1.45, 1.05), 2.3),
        ((-7.15, 1.95, -15.10), (1.25, 1.45, 0.78), 3.7),
        ((-6.20, 2.70, -15.20), (1.28, 1.35, 0.65), 4.4),
        ((-7.35, 3.45, -15.35), (0.95, 1.20, 0.48), 5.1),
        ((-5.05, 0.55, -14.65), (1.30, 0.85, 0.92), 6.0),
        ((6.95, 0.72, -14.25), (1.48, 1.20, 1.18), 7.2),
        ((5.95, 1.18, -14.80), (1.60, 1.42, 1.02), 8.1),
        ((7.10, 1.90, -15.05), (1.22, 1.38, 0.75), 9.0),
        ((6.15, 2.58, -15.28), (1.22, 1.28, 0.60), 10.1),
        ((7.30, 3.25, -15.38), (0.88, 1.12, 0.45), 11.2),
        ((4.55, 0.52, -14.55), (1.25, 0.82, 0.90), 12.3),
        ((-3.15, 0.42, -15.05), (1.15, 0.70, 0.66), 13.4),
        ((3.05, 0.40, -14.92), (1.12, 0.66, 0.68), 14.5),
    )
    for index, (center, size, seed) in enumerate(rock_specs, 1):
        add_faceted_rock(center, size, seed)
    features.append({"type": "hero_tank_rocks", "count": len(rock_specs)})

    # Keep the four viewing benches, but pull them out of the continuous side
    # circulation lanes.  Their long axis now faces the tank; both wall-side
    # routes retain more than three metres of clear width.
    add_bench("HeroHall_Bench_L1", -7.0, -2.25, False)
    add_bench("HeroHall_Bench_L2", -7.0, -5.65, False)
    add_bench("HeroHall_Bench_R1", 7.0, -2.25, False)
    add_bench("HeroHall_Bench_R2", 7.0, -5.65, False)

    # Straight side gallery: west is opaque architecture, east is side glass.
    add_floor("HeroGallery_Floor", -10.05, -12.0, 2.90, 8.0,
              material="WatatsumiRamp")
    add_box("ReceptionWall", "HeroGallery_WestWall", (-11.50, 2.65, -12.0),
            (0.30, 5.30, 8.0), "Solid")
    add_box("ReceptionWall", "HeroGallery_Ceiling", (-10.05, 5.30, -12.0),
            (2.90, 0.24, 8.0), None)
    add_box("WatatsumiGlass", "HeroGallery_SideAcrylic", (-8.60, 2.65, -12.0),
            (0.10, 5.30, 7.70), "Glass")
    # Match the front pane's two-layer optical stack.  Previously this side
    # pane had glass only, so it lacked the same water absorption/refraction
    # and looked like a different material despite sharing its material name.
    add_box("WatatsumiWater", "HeroGallery_SideWaterInterface",
            (-8.72, 2.65, -12.0), (0.05, 5.30, 7.70), None)
    features.append({"type": "side_glass_stack",
                     "layers": ["WatatsumiGlass", "WatatsumiWater"]})
    add_box("WatatsumiArchitecture", "HeroGallery_SideSill", (-8.55, 0.17, -12.0),
            (0.34, 0.34, 8.0), "Solid")
    add_box("WatatsumiArchitecture", "HeroGallery_SideHeader", (-8.55, 5.05, -12.0),
            (0.34, 0.50, 8.0), "Solid")
    # Gallery ceiling is lower than the monumental hall.  Fill the transom at
    # the height change so the side route reads as a deliberate doorway, not a
    # black opening above an unfinished corridor.
    add_box("ReceptionWall", "HeroGallery_EntranceUpperSeal",
            (-10.05, 7.475, -7.92), (2.90, 4.35, 0.24), "Solid")
    # Cover the coplanar hall/gallery floor hand-off.  In the dark route the
    # two independently generated slab edges read as a triangular hole at
    # grazing angles even though collision was continuous.  This 20 mm visual
    # threshold overlaps both slabs without introducing a collision step.
    add_box("WatatsumiArchitecture", "HeroGallery_EntranceThreshold",
            (-10.05, 0.01, -8.0), (2.90, 0.02, 0.36), None)

    # Dark vestibule begins immediately after the tank, with a centred future
    # underwater-arch portal. It is a real room, not an arbitrary bent link.
    add_floor("BlackoutVestibule_Floor", -10.05, -17.75, 2.90, 3.50,
              material="WatatsumiRamp")
    add_box("ReceptionWall", "Blackout_WestWall", (-11.50, 2.65, -17.75),
            (0.30, 5.30, 3.50), "Solid")
    add_box("ReceptionWall", "Blackout_EastWall", (-8.60, 2.65, -17.75),
            (0.30, 5.30, 3.50), "Solid")
    add_box("ReceptionWall", "Blackout_Ceiling", (-10.05, 5.30, -17.75),
            (2.90, 0.24, 3.50), None)
    # The gallery ceiling is 1.30 m higher than the blackout vestibule.  The
    # former bare slab transition exposed the tank/service voids from inside
    # the dark room.  Build a complete portal reveal whose jambs overlap both
    # wall runs slightly, so rasterisation cannot reopen a hairline seam.
    add_box("ReceptionWall", "Blackout_GalleryWestJamb",
            (-11.50, 2.00, -16.0), (0.36, 4.00, 0.50), "Solid")
    add_box("ReceptionWall", "Blackout_GalleryEastJamb",
            (-8.60, 2.00, -16.0), (0.36, 4.00, 0.50), "Solid")
    add_box("ReceptionWall", "Blackout_GalleryHeader",
            (-10.05, 4.65, -16.0), (3.26, 1.30, 0.50), None)
    # Keep the visible header low while giving the 1.95 m capsule ten
    # centimetres of clearance. Collision begins inside the opaque upper part.
    add_collider_box("Blackout_GalleryHeaderClearance",
                     (-10.05, 4.80, -16.0), (3.26, 1.00, 0.50), "Solid")
    # Continue the tank-side jamb back into the gallery. Without this return,
    # the bright side acrylic remained visible from the blackout room even
    # though the doorway frame itself was closed.
    add_box("ReceptionWall", "Blackout_TankOcclusionWing",
            (-8.60, 2.65, -15.0), (0.36, 5.30, 2.00), "Solid")
    features.append({"type": "blackout_gallery_portal",
                     "visual_header_bottom": 1.75,
                     "collision_clearance": 2.05,
                     "tank_occlusion_depth": 2.0})
    add_box("WatatsumiArchitecture", "ArchPortal_Header", (-10.05, 4.40, -19.50),
            (2.90, 1.80, 0.34), "Solid")
    add_box("WatatsumiArchitecture", "ArchPortal_Left", (-11.27, 2.0, -19.50),
            (0.46, 4.0, 0.34), "Solid")
    add_box("WatatsumiArchitecture", "ArchPortal_Right", (-8.83, 2.0, -19.50),
            (0.46, 4.0, 0.34), "Solid")
    # A short compression chamber preserves the blackout beat before the
    # reused 48 m tunnel opens to its full 6.4 m walkway.  It is deliberately
    # straight and shares the vestibule floor datum: no teleport, step or
    # arbitrary bent connector.
    add_floor("ArchConnector_Floor", -10.05, -22.25, 6.40, 5.50,
              material="WatatsumiRamp")
    add_box("ReceptionWall", "ArchConnector_WestWall",
            (-13.25, 2.65, -22.25), (0.30, 5.30, 5.50), "Solid")
    add_box("ReceptionWall", "ArchConnector_EastWall",
            (-6.85, 2.65, -22.25), (0.30, 5.30, 5.50), "Solid")
    add_box("ReceptionWall", "ArchConnector_Ceiling",
            (-10.05, 5.30, -22.25), (6.40, 0.24, 5.50), None)
    add_box("WatatsumiArchitecture", "ArchConnector_EntranceHeader",
            (-10.05, 4.40, -19.62), (2.90, 1.80, 0.18), None)
    # Close both shoulders where the 2.9 m blackout corridor opens into the
    # 6.4 m arch connector.  These were literal walk-through holes beside the
    # portal, most visible on the player's left after entering the connector.
    add_box("ReceptionWall", "ArchConnector_WestReturn",
            (-12.30, 2.00, -19.50), (1.60, 4.00, 0.40), "Solid")
    add_box("ReceptionWall", "ArchConnector_EastReturn",
            (-7.80, 2.00, -19.50), (1.60, 4.00, 0.40), "Solid")

    # Compact V3 horseshoe. One sampled centre line drives the visible floor,
    # arched shell and runtime path collision, eliminating the old three-run
    # layout's mismatched corners and 24 m-long footprint.
    ramp_points = build_compact_ramp_points()
    add_path_strip("HeroRampFloor", "HeroRamp_Main", ramp_points, RAMP_WIDTH)
    add_path_arch_shell("ReceptionWall", "HeroRamp_ArchShell",
                        ramp_points, RAMP_WIDTH,
                        clear_height=RAMP_CLEAR_HEIGHT,
                        spring_height=1.55, arch_segments=10)
    # Six sparse practicals reproduce key 6's rhythm. They are emissive mesh
    # only, so the route gains depth cues without six additional dynamic lights.
    for light_index, point_index in enumerate(tuple(int(len(ramp_points)*i/7) for i in range(1,7)), 1):
        point = ramp_points[min(point_index, len(ramp_points) - 2)]
        add_box("WatatsumiEmitter", f"HeroRamp_CeilingCue_{light_index}",
                (point[0], point[1] + RAMP_CLEAR_HEIGHT - 0.18, point[2]),
                (0.72, 0.035, 0.14), None)
    # Reception route coordinates for the reused tunnel.  StageModel rotates
    # the visual asset so its local +X axis becomes runtime +Z; collision is
    # emitted directly in that final space from the same 48 m profile.
    arch_points = []
    for index in range(193):
        t = index / 192.0
        smooth = t * t * (3.0 - 2.0 * t)
        arch_points.append((-10.05, -4.70 * smooth, -(25.0 + 48.0 * t)))
    add_runtime_path("Reception_UnderwaterArch_Walkway", arch_points, 3.06)
    # The arched shell is the exterior architecture on the wrap-around section;
    # the path collider itself enforces both edges.  Avoid enclosing it again
    # in a rectangular service box, which was the source of the former crude
    # room-within-a-room silhouette.
    # No decorative threshold is layered over the ramp.  The old coplanar
    # slabs fought the ramp top in the depth buffer and produced flicker.

    # Midpoint exhibit is recessed into the landing wall, not placed in the
    # walking envelope. The blue pane also acts as a navigation landmark.
    add_box("WatatsumiArchitecture", "RampMid_DisplayRecess",
            (15.72, 2.90, -10.4), (0.22, 1.85, 4.80), None)
    add_box("WatatsumiWater", "RampMid_DisplayWater", (15.57, 2.90, -10.4),
            (0.05, 1.25, 4.20), None)
    add_box("WatatsumiGlass", "RampMid_DisplayGlass", (15.49, 2.90, -10.4),
            (0.06, 1.25, 4.20), "Glass")
    add_box("WatatsumiEmitter", "RampMid_DisplayTopLight",
            (15.43, 3.49, -10.4), (0.05, 0.035, 3.82), None)
    # Deep metal reveal and four-piece acrylic frame make the display read as
    # part of the wall instead of a luminous rectangle pasted over it.
    add_box("WatatsumiArchitecture", "RampMid_DisplayFrameTop",
            (15.39, 3.59, -10.4), (0.18, 0.18, 4.56), None)
    add_box("WatatsumiArchitecture", "RampMid_DisplayFrameBottom",
            (15.39, 2.21, -10.4), (0.18, 0.18, 4.56), None)
    add_box("WatatsumiArchitecture", "RampMid_DisplayFrameNear",
            (15.39, 2.90, -8.12), (0.18, 1.56, 0.18), None)
    add_box("WatatsumiArchitecture", "RampMid_DisplayFrameFar",
            (15.39, 2.90, -12.68), (0.18, 1.56, 0.18), None)
    add_box("ReceptionMetal", "RampMid_DisplaySillCap",
            (15.29, 2.33, -10.4), (0.08, 0.05, 4.36), None)
    features.append({"type": "ramp_intermediate_exhibit",
                     "elevation": 2.90, "dimensions": [4.2, 1.25]})

    # Spacious U/H gallery matching the approved concept image.  Its side
    # arms clear the complete tank frame and the tank-facing bridge remains
    # narrow enough that the 1F viewing hall reads as an atrium below.
    upper_y = 5.50
    # Three-piece H deck. Each side arm is one continuous slab and the cross
    # piece only spans the space between them. The former five-piece version
    # met at two long coplanar edges; their dark side faces leaked through as
    # the black floor overlap visible opposite the management/exhibit wall.
    for side, x in (("Right", 11.85), ("Left", -11.85)):
        add_floor(f"Upper_{side}Arm", x, -8.50,
                  4.80, 17.00, 5.50, "HeroRampFloor")
    add_floor("Upper_CrossBridge", 0.0, -4.125,
              18.90, 6.45, 5.50, "HeroRampFloor")
    add_box("ReceptionWall", "Upper_H_Ceiling",
            (0.0, 11.50, -9.00), (29.10, 0.30, 18.00), None)
    outer_x = 14.25
    upper_wall_height = 5.85
    upper_wall_y = 8.425
    add_box("ReceptionWall", "Upper_RightOuterWall",
            (14.25, 8.425, -9.00), (0.30, 5.85, 18.00), "Solid")
    add_box("ReceptionWall", "Upper_LeftOuterWall",
            (-14.25, 8.425, -11.85),
            (0.30, 5.85, 12.30), "Solid")
    add_box("ReceptionWall", "Upper_LeftOuterWallFront",
            (-14.25, 8.425, -0.45), (0.30, 5.85, 0.90), "Solid")
    add_box("ReceptionWall", "Upper_NorthWall", (0.0, 8.425, -18.00),
            (28.50, 5.85, 0.30), "Solid")

    # Solid lower guards with metal caps match the approved gallery image.
    # The north guard sits before the acrylic reveal rather than inside it.
    parapet_y = upper_y + 0.525
    inner_x = UPPER_ARM_CENTER_X - UPPER_ARM_WIDTH * 0.5
    bridge_half_depth = UPPER_GALLERY_DEPTH * 0.5
    bridge_south = UPPER_CROSS_Z + bridge_half_depth
    bridge_north = UPPER_CROSS_Z - bridge_half_depth
    near_length = abs(bridge_south)
    far_length = UPPER_DEPTH + bridge_north
    for name, center, size in (
        ("Upper_RightInnerParapetNear", (inner_x, parapet_y, bridge_south * 0.5),
         (0.18, 1.05, near_length)),
        ("Upper_RightInnerParapetFar", (inner_x, parapet_y,
         (bridge_north - UPPER_DEPTH) * 0.5), (0.18, 1.05, far_length)),
        ("Upper_LeftInnerParapetNear", (-inner_x, parapet_y, bridge_south * 0.5),
         (0.18, 1.05, near_length)),
        ("Upper_LeftInnerParapetFar", (-inner_x, parapet_y,
         (bridge_north - UPPER_DEPTH) * 0.5), (0.18, 1.05, far_length)),
        ("Upper_CrossSouthParapet", (0.0, parapet_y,
         bridge_south), (inner_x * 2.0, 1.05, 0.18)),
        ("Upper_CrossNorthParapet", (0.0, parapet_y,
         bridge_north), (inner_x * 2.0, 1.05, 0.18)),
    ):
        if name == "Upper_CrossNorthParapet":
            # Keep the lower reef visible through a public-grade glazed guard.
            # Its invisible collision remains at the full 1.05 m guard height.
            overlook_z = center[2] + 0.20
            add_box("WatatsumiArchitecture", name,
                    (center[0], upper_y + 0.16, overlook_z),
                    (size[0], 0.32, size[2]), None)
            add_box("TankGlassReception", f"{name}_SafetyGlass",
                    (center[0], upper_y + 0.67, overlook_z),
                    (size[0] - 0.18, 0.70, 0.055), None)
            add_collider_box(f"{name}_GuardCollision",
                             (center[0], upper_y + 0.525, overlook_z),
                             size, "Solid")
            center = (center[0], center[1], overlook_z)
        else:
            add_box("WatatsumiArchitecture", name, center, size, "Solid")
        cap_center = (center[0], upper_y + 1.07, center[2])
        cap_size = (size[0] + (0.10 if size[0] < 1.0 else 0.0), 0.05,
                    size[2] + (0.10 if size[2] < 1.0 else 0.0))
        add_box("ReceptionMetal", f"{name}_Cap", cap_center, cap_size, None)
    # The old low-mounted wayfinding cards read as loose blue rectangles on
    # the floor and at the exhibit entrances. Lighting comes from the analytic
    # rig, so removing every card costs no actual illumination.

    # Future room portals are fully framed and presently closed, preventing
    # accidental falls into unbuilt space while keeping expansion coordinates.
    for name, x, z, sx, sz in (
        ("Terrace", -outer_x + 0.16, -8.0, 0.24, 3.2),
        ("Dolphin", outer_x - 0.16, -8.5, 0.24, 3.2),
        ("Management", UPPER_ARM_CENTER_X, -UPPER_DEPTH + 0.16, 3.2, 0.24),
        ("FutureExhibit", outer_x - 0.16, -13.5, 0.24, 3.2),
    ):
        add_box("Door", f"UpperPortal_{name}", (x, upper_y + 1.55, z),
                (sx, 3.10, sz), "Solid")
    features.extend((
        {"type": "hero_tank", "pane": [17.0, HERO_TANK_HEIGHT],
         "depth": 8.0, "surface_y": HERO_TANK_SURFACE_Y,
         "upper_floor_y": UPPER_FLOOR_Y,
         "surface_hidden": True},
        {"type": "straight_side_gallery", "length": 8.0},
        {"type": "blackout_vestibule", "length": 3.5},
        {"type": "ramp", "rise": RAMP_RISE, "width": RAMP_WIDTH,
         "clear_height": RAMP_CLEAR_HEIGHT,
         "entrance_width": RAMP_ENTRANCE_WIDTH,
         "maximum_slope": max(abs(b[1]-a[1])/max(math.hypot(b[0]-a[0],b[2]-a[2]),.0001)
                              for a,b in zip(ramp_points,ramp_points[1:])),
         "runs": 3, "layout": "watatsumi_wrap"},
        {"type": "upper_h_walkway", "elevation": UPPER_FLOOR_Y,
         "portals": 4, "arm_width": UPPER_ARM_WIDTH,
         "depth": UPPER_DEPTH, "observation_depth": UPPER_GALLERY_DEPTH,
         "observation_width": UPPER_ARM_CENTER_X * 2.0 + UPPER_ARM_WIDTH,
         "tank_reveal_gap": 0.65},
        {"type": "lighting_modes", "values": ["Blue", "White", "Red"],
         "default": "Blue"},
    ))


def build() -> None:
    for name, rgba in {
        "ReceptionFloor": (0.34, 0.38, 0.41, 1.0),
        "ReceptionWall": (0.030, 0.055, 0.082, 1.0),
        "ReceptionCounter": (0.30, 0.39, 0.44, 1.0),
        "TankGlassReception": (0.10, 0.22, 0.28, 0.16),
        "ReceptionMetal": (0.045, 0.075, 0.095, 1.0),
        "ManagementDoor": (0.045, 0.070, 0.085, 1.0),
        "MissionManualPaper": (0.72, 0.76, 0.68, 1.0),
        "EmissiveEmergencyGreen": (0.08, 0.82, 0.32, 1.0),
        "WatatsumiArchitecture": (0.020, 0.028, 0.038, 1.0),
        "WatatsumiRamp": (0.040, 0.050, 0.060, 1.0),
        "HeroRampFloor": (0.045, 0.065, 0.082, 1.0),
        "WatatsumiRock": (0.025, 0.045, 0.055, 1.0),
        "WatatsumiWater": (0.010, 0.135, 0.245, 0.84),
        "WatatsumiGlass": (0.055, 0.220, 0.310, 0.16),
        "WatatsumiWaterSurface": (0.025, 0.300, 0.430, 0.58),
        "WatatsumiBubble": (0.180, 0.620, 1.000, 0.18),
        "WatatsumiEmitter": (0.080, 0.640, 0.920, 1.0),
    }.items():
        ensure_material(name, rgba)
    reset()

    # One unbroken support plane prevents the entrance and gate area from
    # becoming separate collision islands.
    add_box("ReceptionFloor", "Reception_Floor",
            (0.0, -0.09, ROOM_DEPTH * 0.5),
            (ROOM_WIDTH, 0.18, ROOM_DEPTH), None)
    floor_rects.append({
        "name": "Reception_Floor", "minimum_x": -8.5,
        "maximum_x": 8.5, "minimum_z": -8.5, "maximum_z": 0.0,
        "floor_y": FLOOR_OFFSET})
    add_box("Ceiling", "Reception_Ceiling",
            (0.0, ROOM_HEIGHT, ROOM_DEPTH * 0.5),
            (ROOM_WIDTH, 0.22, ROOM_DEPTH), None)
    add_curved_counter_soffit()

    add_wall_z("Reception_WestWall", -8.5, 0.0, 8.5)
    add_wall_z("Reception_EastWall", 8.5, 0.0, 8.5)

    # South exterior wall: only the central double glass door is open in the
    # masonry. The dark exterior remains visible through the locked panes.
    add_wall_x("Reception_SouthWallLeft", -8.5, -1.55, 8.5)
    add_wall_x("Reception_SouthWallRight", 1.55, 8.5, 8.5)
    add_wall_x("Reception_SouthDoorHeader", -1.55, 1.55, 8.5, 3.15, 1.35)
    add_box("TankGlassReception", "Reception_ExteriorGlassDoor",
            (0.0, 1.57, 8.47), (3.02, 3.14, 0.10), "Glass")
    add_box("ReceptionMetal", "Reception_ExteriorDoorMullion",
            (0.0, 1.57, 8.39), (0.055, 3.14, 0.08), "Solid")
    # Visible pull handles match the broad runtime interaction region.
    for side in (-1, 1):
        add_box("ReceptionMetal", "Reception_ExteriorHandle" + str(side),
                (side * .25, 1.30, 8.23), (.045, .65, .065))
        for y in (1.05, 1.55):
            add_box("ReceptionMetal", "Reception_ExteriorHandleMount",
                    (side * .25, y, 8.31), (.06, .06, .18))

    # North wall: a framed 6.0 m tank/gate opening and one 1.2 m staff door. Every
    # centimetre beside that staff door is solid wall; this removes the stray
    # void called out in review.
    add_wall_x("Reception_NorthWallWest", -8.5, -3.0, 0.0)
    add_wall_x("Reception_NorthGateHeader", -3.0, 3.0, 0.0, 3.12, 1.38)
    add_wall_x("Reception_NorthServiceLeft", 3.0, 6.15, 0.0)
    add_wall_x("Reception_NorthServiceDoorHeader", 6.15, 7.35, 0.0, 2.95, 1.55)
    add_wall_x("Reception_NorthServiceRight", 7.35, 8.5, 0.0)
    add_box("ManagementDoor", "Reception_StaffDoor",
            (6.75, 1.475, 0.02), (1.14, 2.95, 0.10), "Solid")
    add_box("ReceptionMetal", "Reception_StaffDoorLeftTrim",
            (6.11, 1.50, 0.13), (0.12, 3.12, 0.18), None)
    add_box("ReceptionMetal", "Reception_StaffDoorRightTrim",
            (7.39, 1.50, 0.13), (0.12, 3.12, 0.18), None)
    add_box("ReceptionMetal", "Reception_StaffDoorTopTrim",
            (6.75, 3.01, 0.13), (1.40, 0.12, 0.18), None)
    add_box("TankGlassReception", "Reception_StaffDoorWindow",
            (6.97, 1.70, 0.10), (0.16, 1.10, 0.08), None)
    add_box("ManagementDoor", "Reception_StaffDoorHandle",
            (6.35, 1.20, 0.14), (0.06, 0.08, 0.18), None)
    # Matching hall-side hardware keeps the service door from reading as a
    # blank prop when approached from the hero-tank floor.
    add_box("ReceptionMetal", "HeroHall_StaffDoorLeftTrim",
            (6.11, 1.50, -0.13), (0.12, 3.12, 0.18), None)
    add_box("ReceptionMetal", "HeroHall_StaffDoorRightTrim",
            (7.39, 1.50, -0.13), (0.12, 3.12, 0.18), None)
    add_box("ReceptionMetal", "HeroHall_StaffDoorTopTrim",
            (6.75, 3.01, -0.13), (1.40, 0.12, 0.18), None)
    add_box("TankGlassReception", "HeroHall_StaffDoorWindow",
            (6.97, 1.70, -0.10), (0.16, 1.10, 0.08), None)
    add_box("ManagementDoor", "HeroHall_StaffDoorHandle",
            (6.35, 1.20, -0.14), (0.06, 0.08, 0.18), None)

    # The framed opening remains genuinely open for the future hero-tank room;
    # no fake aquarium backdrop is drawn here.
    add_box("ReceptionMetal", "Reception_TankFrameLeftOuter",
            (-3.10, 1.58, 0.14), (0.20, 3.18, 0.28), None)
    add_box("ReceptionMetal", "Reception_TankFrameRightOuter",
            (3.10, 1.58, 0.14), (0.20, 3.18, 0.28), None)
    add_box("ReceptionMetal", "Reception_TankFrameTopOuter",
            (0.0, 3.17, 0.14), (6.40, 0.20, 0.28), None)
    add_box("ReceptionCounter", "Reception_TankFrameLeftInner",
            (-2.82, 1.50, 0.20), (0.16, 2.72, 0.22), None)
    add_box("ReceptionCounter", "Reception_TankFrameRightInner",
            (2.82, 1.50, 0.20), (0.16, 2.72, 0.22), None)
    add_box("ReceptionCounter", "Reception_TankFrameTopInner",
            (0.0, 2.85, 0.20), (5.80, 0.16, 0.22), None)

    # Three staffed-style inspection desks with wide, freely walkable lanes.
    for index, x in enumerate((-2.35, 0.0, 2.35), start=1):
        add_ticket_desk(index, x)
    features.append({"type": "ticket_desks", "count": 3})

    add_curved_reception_counter()
    add_curved_reception_screen()
    # The missing operations manual is visibly scattered on the staff-side
    # floor. Runtime story state hides all sheets once the stack is collected.
    add_box("MissionManualPaper", "Reception_ManualSheet_A",
            (5.55, 0.025, 1.46), (0.62, 0.05, 0.42), None)
    add_box("MissionManualPaper", "Reception_ManualSheet_B",
            (6.18, 0.030, 1.84), (0.44, 0.06, 0.64), None)
    add_box("MissionManualPaper", "Reception_ManualSheet_C",
            (5.78, 0.035, 2.20), (0.68, 0.07, 0.40), None)
    features.append({"type": "curved_reception_counter", "count": 1})
    features.append({"type": "curved_reception_acrylic", "panels": 24,
                     "collision_segments": 8})
    features.append({"type": "staff_door", "count": 1,
                     "surrounding_wall_gap": 0.0})

    # One live emergency fixture; the other ceiling discs are intentionally
    # dark architectural props for the closed-aquarium state.
    add_box("EmissiveEmergencyGreen", "Reception_EmergencyLight",
            (6.75, 3.28, 0.18), (0.46, 0.18, 0.08), None)
    downlights = ((-5.0, 4.8), (-2.5, 4.8), (0.0, 4.8),
                  (2.5, 4.8), (5.0, 4.8), (-3.8, 2.4),
                  (-0.8, 2.4), (2.2, 2.4), (5.6, 2.6), (7.8, 2.2))
    for index, (x, z) in enumerate(downlights, start=1):
        route.add_cylinder("ReceptionMetal", f"Reception_Downlight_{index}",
                           (x, 4.38, z), 0.11, 0.05, 16)
    features.append({"type": "active_emergency_lights", "count": 1})
    features.append({"type": "inactive_downlights", "count": len(downlights)})
    build_integrated_hero_tank()
    from upper_floor_v4 import build_upper
    build_upper(__import__(__name__))


def f(value: float) -> str:
    return f"{0.0 if abs(value) < 0.00005 else value:.5f}f"


def write_header() -> None:
    OUTPUT_HEADER.parent.mkdir(parents=True, exist_ok=True)
    tags = {"Solid": 2, "Glass": 5}
    lines = [
        "// Generated by tools/generate_reception_lobby.py. Do not hand edit.",
        "#pragma once", "#include <array>", "#include <cstddef>", "#include <cstdint>",
        "namespace reception_lobby", "{",
        "struct BoxSpec { const wchar_t* name; float minX, minY, minZ; float maxX, maxY, maxZ; std::uint8_t tag; };",
        "struct FloorSpec { const wchar_t* name; float minX, maxX, minZ, maxZ, floorY; };",
        "struct PathPoint { float x, y, z; };",
        "struct PathSpec { const wchar_t* name; const PathPoint* points; std::size_t count; float halfWidth; };",
        f"inline constexpr std::array<BoxSpec, {len(boxes)}> kBoxes{{{{",
    ]
    for box in boxes:
        mn, mx = box.minimum, box.maximum
        lines.append(f'    {{L"{box.name}", {f(mn[0])}, {f(mn[1])}, {f(mn[2])}, '
                     f'{f(mx[0])}, {f(mx[1])}, {f(mx[2])}, {tags[box.tag]}}},')
    lines += ["}};", f"inline constexpr std::array<FloorSpec, {len(floor_rects)}> kFloors{{{{"]
    for floor in floor_rects:
        lines.append(f'    {{L"{floor["name"]}", {f(floor["minimum_x"])}, '
                     f'{f(floor["maximum_x"])}, {f(floor["minimum_z"])}, '
                     f'{f(floor["maximum_z"])}, {f(floor["floor_y"])}}},')
    lines.append("}};")
    for index, path in enumerate(paths):
        lines.append(f"inline constexpr std::array<PathPoint, {len(path.points)}> kPath{index}Points{{{{")
        for point in path.points:
            lines.append(f"    {{{f(point[0])}, {f(point[1])}, {f(point[2])}}},")
        lines.append("}};")
    lines.append(f"inline constexpr std::array<PathSpec, {len(paths)}> kPaths{{{{")
    for index, path in enumerate(paths):
        lines.append(f'    {{L"{path.name}", kPath{index}Points.data(), '
                     f'kPath{index}Points.size(), {f(path.half_width)}}},')
    lines += ["}};", "}"]
    OUTPUT_HEADER.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_manifest(statistics: dict) -> None:
    OUTPUT_MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_MANIFEST.write_text(json.dumps({
        "version": 3, "units": "metres",
        "room": {"width": ROOM_WIDTH, "depth": ROOM_DEPTH,
                 "height": ROOM_HEIGHT},
        "boxes": [box.__dict__ for box in boxes],
        "floors": floor_rects,
        "paths": [{"name": path.name, "points": path.points,
                   "half_width": path.half_width} for path in paths],
        "zones": ["reception", "hero_hall", "hero_tank",
                  "straight_side_gallery", "blackout_vestibule",
                  "ramp", "upper_h_walkway"],
        "features": features,
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
                      "colliders": len(boxes), **stats}, ensure_ascii=False))
