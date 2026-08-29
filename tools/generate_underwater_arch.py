"""Generate a descending underwater-arch greybox as binary glTF.

One unit is one meter. The route runs along +X and descends 4.7 meters over
48 meters. Stable mesh/material names are intentional authoring hand-off IDs.
"""

from __future__ import annotations

import json
import math
import struct
from dataclasses import dataclass, field
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_GLB = ROOT / "model" / "aquarium_underwater_arch.glb"
OUTPUT_PLAN = ROOT / "concepts" / "underwater-arch-layout.png"
ARCH_SPRING_HEIGHT = 1.20
ARCH_WATER_RADIUS = 3.58
ARCH_GLASS_RADIUS = 3.42
ARCH_RIB_RADIUS = 3.36
ARCH_WATER_HEIGHT = 3.88
ARCH_GLASS_HEIGHT = 3.72
ARCH_RIB_HEIGHT = 3.66
WALKWAY_HALF_WIDTH = 3.20
WATER_SURFACE_HEIGHT = 5.80


@dataclass
class MeshGroup:
    name: str
    positions: list[float] = field(default_factory=list)
    normals: list[float] = field(default_factory=list)
    texcoords: list[float] = field(default_factory=list)
    indices: list[int] = field(default_factory=list)


MATERIALS = {
    "ArchFloor": (0.018, 0.030, 0.045, 1.0),
    "Metal": (0.055, 0.085, 0.115, 1.0),
    "ArchRail": (0.055, 0.105, 0.145, 1.0),
    "ArchTrim": (0.120, 0.220, 0.285, 1.0),
    "ArchSeam": (0.018, 0.100, 0.175, 1.0),
    "ArchRock": (0.018, 0.050, 0.072, 1.0),
    "EmissiveCyan": (0.018, 0.550, 1.000, 1.0),
    "ArchWaterSurface": (0.040, 0.360, 0.560, 0.32),
    "TankWaterArch": (0.008, 0.120, 0.300, 0.58),
    "TankGlassArch": (0.055, 0.230, 0.400, 0.16),
    "ArchBubble": (0.260, 0.720, 1.000, 0.20),
    "ArchLightCurtain": (0.090, 0.420, 1.000, 0.12),
}
groups = {name: MeshGroup(name) for name in MATERIALS}


def append_quad(material, vertices, normal, uvs=((0, 0), (0, 1), (1, 1), (1, 0))):
    group = groups[material]
    base = len(group.positions) // 3
    for vertex, uv in zip(vertices, uvs):
        group.positions.extend(vertex)
        group.normals.extend(normal)
        group.texcoords.extend(uv)
    group.indices.extend((base, base + 1, base + 2, base, base + 2, base + 3))


def append_quad_smooth(material, vertices, normals, uvs):
    """Append a quad with independent per-vertex normals for curved shells."""
    group = groups[material]
    base = len(group.positions) // 3
    for vertex, normal, uv in zip(vertices, normals, uvs):
        group.positions.extend(vertex)
        group.normals.extend(normal)
        group.texcoords.extend(uv)
    group.indices.extend((base, base + 1, base + 2, base, base + 2, base + 3))


def add_box(material, center, size):
    cx, cy, cz = center
    sx, sy, sz = (value * 0.5 for value in size)
    p = [
        (cx - sx, cy - sy, cz - sz), (cx + sx, cy - sy, cz - sz),
        (cx + sx, cy + sy, cz - sz), (cx - sx, cy + sy, cz - sz),
        (cx - sx, cy - sy, cz + sz), (cx + sx, cy - sy, cz + sz),
        (cx + sx, cy + sy, cz + sz), (cx - sx, cy + sy, cz + sz),
    ]
    faces = [
        ((0, 3, 2, 1), (0, 0, -1)), ((4, 5, 6, 7), (0, 0, 1)),
        ((0, 4, 7, 3), (-1, 0, 0)), ((1, 2, 6, 5), (1, 0, 0)),
        ((3, 7, 6, 2), (0, 1, 0)), ((0, 1, 5, 4), (0, -1, 0)),
    ]
    for corners, normal in faces:
        append_quad(material, [p[index] for index in corners], normal)


def add_slope_profile(material, x0, x1, profile, side):
    """Extrude a beveled Y/Z profile along a descending route segment."""
    floor0 = route_height(x0)
    floor1 = route_height(x1)
    ring_size = len(profile)

    for index in range(ring_size):
        next_index = (index + 1) % ring_size
        y0, z0 = profile[index]
        y1, z1 = profile[next_index]
        edge_y = y1 - y0
        edge_z = (z1 - z0) * side
        normal_length = max(math.hypot(edge_z, edge_y), 0.0001)
        normal = (0.0, edge_z / normal_length, -edge_y / normal_length)
        append_quad_smooth(
            material,
            [(x0, floor0 + y0, z0 * side),
             (x0, floor0 + y1, z1 * side),
             (x1, floor1 + y1, z1 * side),
             (x1, floor1 + y0, z0 * side)],
            (normal, normal, normal, normal),
            ((0.0, 0.0), (0.0, 1.0), (1.0, 1.0), (1.0, 0.0)))


def add_ellipsoid(material, center, radii, latitude_segments=8,
                  longitude_segments=14, seed=0.0):
    """Add a low-poly rounded rock silhouette without box-shaped corners."""
    group = groups[material]
    cx, cy, cz = center
    rx, ry, rz = radii
    base = len(group.positions) // 3
    for latitude in range(latitude_segments + 1):
        v = latitude / latitude_segments
        phi = math.pi * v
        sin_phi = math.sin(phi)
        cos_phi = math.cos(phi)
        for longitude in range(longitude_segments + 1):
            u = longitude / longitude_segments
            theta = math.tau * u
            dx = sin_phi * math.cos(theta)
            dy = cos_phi
            dz = sin_phi * math.sin(theta)
            roughness = (
                1.0
                + sin_phi * 0.12 * math.sin(theta * 3.0 + seed * 1.37)
                + sin_phi * 0.075 * math.sin(
                    theta * 5.0 - phi * 2.0 + seed * 2.11)
                + 0.055 * math.cos(phi * 3.0 + seed)
            )
            group.positions.extend((
                cx + dx * rx * roughness,
                cy + dy * ry * roughness,
                cz + dz * rz * roughness))
            nx, ny, nz = dx / rx, dy / ry, dz / rz
            inverse_length = 1.0 / max(math.sqrt(nx * nx + ny * ny + nz * nz), 0.0001)
            group.normals.extend((nx * inverse_length, ny * inverse_length, nz * inverse_length))
            group.texcoords.extend((u, v))
    row = longitude_segments + 1
    for latitude in range(latitude_segments):
        for longitude in range(longitude_segments):
            a = base + latitude * row + longitude
            b = a + row
            group.indices.extend((a, b, a + 1, a + 1, b, b + 1))


def add_water_surface_grid(x_segments=48, z_segments=24):
    """Add the real tank surface above the acrylic tunnel crown."""
    group = groups["ArchWaterSurface"]
    base = len(group.positions) // 3
    x0, x1 = -2.0, 50.0
    z0, z1 = -9.0, 9.0
    for xi in range(x_segments + 1):
        u = xi / x_segments
        x = x0 + (x1 - x0) * u
        for zi in range(z_segments + 1):
            v = zi / z_segments
            z = z0 + (z1 - z0) * v
            group.positions.extend((x, WATER_SURFACE_HEIGHT, z))
            group.normals.extend((0.0, -1.0, 0.0))
            group.texcoords.extend((u, v))
    row = z_segments + 1
    for xi in range(x_segments):
        for zi in range(z_segments):
            a = base + xi * row + zi
            b = a + row
            group.indices.extend((a, a + 1, b, a + 1, b + 1, b))


def add_light_card(top, bottom, top_width, bottom_width, across):
    """Append one soft tapered sheet used by the crossed light curtain."""
    ax, ay, az = across
    tx, ty, tz = top
    bx, by, bz = bottom
    vertices = [
        (tx - ax * top_width, ty - ay * top_width, tz - az * top_width),
        (bx - ax * bottom_width, by - ay * bottom_width, bz - az * bottom_width),
        (bx + ax * bottom_width, by + ay * bottom_width, bz + az * bottom_width),
        (tx + ax * top_width, ty + ay * top_width, tz + az * top_width),
    ]
    direction = (bx - tx, by - ty, bz - tz)
    nx = direction[1] * az - direction[2] * ay
    ny = direction[2] * ax - direction[0] * az
    nz = direction[0] * ay - direction[1] * ax
    inverse_length = 1.0 / max(math.sqrt(nx * nx + ny * ny + nz * nz), 0.0001)
    append_quad(
        "ArchLightCurtain",
        vertices,
        (nx * inverse_length, ny * inverse_length, nz * inverse_length),
        ((0.0, 0.0), (0.0, 1.0), (1.0, 1.0), (1.0, 0.0)))


def add_refracted_light_curtains():
    """Cross two long tapered cards per bank from the surface into side water."""
    banks = (
        (8.0, -2.10, (0.08, -1.0, 0.10)),
        (24.0, 2.00, (-0.06, -1.0, -0.08)),
        (40.0, -1.65, (0.09, -1.0, 0.07)),
    )
    for bank_index, (x, z, direction) in enumerate(banks):
        floor = route_height(x)
        top = (x, WATER_SURFACE_HEIGHT - 0.06, z)
        # Continue the apparent shaft past the acrylic crown and into the tank
        # water beside the dry walkway. The lateral bend keeps the cards out
        # of the player's corridor while making their full length readable
        # through the curved glass.
        side = -1.0 if z < 0.0 else 1.0
        vertical_travel = top[1] - (floor + 0.62)
        bottom = (
            x + direction[0] * vertical_travel + (bank_index - 1) * 0.28,
            floor + 0.62,
            side * (ARCH_GLASS_RADIUS + 1.28))
        add_light_card(top, bottom, 0.34, 1.72, (0.0, 0.0, 1.0))
        add_light_card(top, bottom, 0.28, 1.38, (1.0, 0.0, 0.0))


def add_bubble_plumes():
    """One draw-batch of fine bubbles rising from side diffusers to the surface."""
    banks = (8.0, 24.0, 40.0)
    for bank_index, bank_x in enumerate(banks):
        for side_index, side in enumerate((-1.0, 1.0)):
            plume_z = side * (4.85 + bank_index * 0.12)
            floor = route_height(bank_x) - 0.42
            for bubble_index in range(24):
                u = (bubble_index + 0.35 * side_index) / 23.5
                height_t = 1.0 - pow(1.0 - min(u, 1.0), 1.55)
                phase = (
                    bubble_index * 2.399963 +
                    bank_index * 1.73 + side_index * 0.91)
                x = bank_x + math.sin(phase * 1.31) * (0.18 + 0.20 * height_t)
                z = plume_z + math.cos(phase * 0.87) * (0.14 + 0.24 * height_t)
                y = floor + (WATER_SURFACE_HEIGHT - 0.12 - floor) * height_t
                radius = 0.035 + 0.060 * (0.5 + 0.5 * math.sin(phase * 1.91))
                radius *= 0.76 + height_t * 0.42
                add_ellipsoid(
                    "ArchBubble",
                    (x, y, z),
                    (radius * 0.84, radius * 1.15, radius),
                    latitude_segments=4,
                    longitude_segments=6,
                    seed=phase)


def route_height(x: float) -> float:
    t = max(0.0, min(1.0, x / 48.0))
    smooth = t * t * (3.0 - 2.0 * t)
    return -4.7 * smooth


def add_slope_slab(material, x0, x1, z0, z1, thickness=0.18, lift=0.0):
    y0 = route_height(x0) + lift
    y1 = route_height(x1) + lift
    slope = (y1 - y0) / (x1 - x0)
    inv = 1.0 / math.sqrt(1.0 + slope * slope)
    top_normal = (-slope * inv, inv, 0.0)
    top = [(x0, y0, z0), (x0, y0, z1), (x1, y1, z1), (x1, y1, z0)]
    bottom = [(x0, y0 - thickness, z0), (x1, y1 - thickness, z0),
              (x1, y1 - thickness, z1), (x0, y0 - thickness, z1)]
    append_quad(material, top, top_normal)
    append_quad(material, bottom, (slope * inv, -inv, 0.0))
    append_quad(material, [top[0], bottom[0], bottom[1], top[3]], (0, 0, -1))
    append_quad(material, [top[1], top[2], bottom[2], bottom[3]], (0, 0, 1))


def add_arch_surface(
        material, radius, height,
        x_segments=24, angle_segments=24, inward=True):
    """Add the semicircular water/glass canopy around the walking route."""
    for xi in range(x_segments):
        x0 = 48.0 * xi / x_segments
        x1 = 48.0 * (xi + 1) / x_segments
        for ai in range(angle_segments):
            a0 = math.pi * ai / angle_segments
            a1 = math.pi * (ai + 1) / angle_segments

            def point(x, angle):
                floor = route_height(x)
                return (x, floor + ARCH_SPRING_HEIGHT + math.sin(angle) * height,
                        math.cos(angle) * radius)

            vertices = [point(x0, a0), point(x0, a1), point(x1, a1), point(x1, a0)]
            sign = -1.0 if inward else 1.0
            def ellipse_normal(angle):
                ny = math.sin(angle) / height
                nz = math.cos(angle) / radius
                inverse_length = 1.0 / math.sqrt(ny * ny + nz * nz)
                return (0.0, sign * ny * inverse_length,
                        sign * nz * inverse_length)

            normal0 = ellipse_normal(a0)
            normal1 = ellipse_normal(a1)
            append_quad_smooth(
                material,
                vertices,
                (normal0, normal1, normal1, normal0),
                ((xi / x_segments, ai / angle_segments),
                 (xi / x_segments, (ai + 1) / angle_segments),
                 ((xi + 1) / x_segments, (ai + 1) / angle_segments),
                 ((xi + 1) / x_segments, ai / angle_segments)))


def add_arch_side_walls(material, radius, x_segments=24):
    """Add vertical glass/water walls below the raised arch spring line."""
    for xi in range(x_segments):
        x0 = 48.0 * xi / x_segments
        x1 = 48.0 * (xi + 1) / x_segments
        y00 = route_height(x0)
        y10 = route_height(x1)
        for side in (-1.0, 1.0):
            z = side * radius
            vertices = [
                (x0, y00, z),
                (x0, y00 + ARCH_SPRING_HEIGHT, z),
                (x1, y10 + ARCH_SPRING_HEIGHT, z),
                (x1, y10, z),
            ]
            # Both shells face the visitor route; Stage rendering is
            # double-sided but a coherent normal keeps Fresnel stable.
            normal = (0.0, 0.0, -side)
            append_quad(
                material,
                vertices,
                normal,
                ((xi / x_segments, 0.0),
                 (xi / x_segments, 1.0),
                 ((xi + 1) / x_segments, 1.0),
                 ((xi + 1) / x_segments, 0.0)))


def add_arch_rib(
        x: float, radius=ARCH_RIB_RADIUS,
        height=ARCH_RIB_HEIGHT, segments=28):
    # Keep the structural rib just inside the glass shell. When it sat outside
    # the refractive surface, glass blending visually erased parts of the arc.
    half_width = 0.035
    for ai in range(segments):
        a0 = math.pi * ai / segments
        a1 = math.pi * (ai + 1) / segments

        def point(px, angle):
            floor = route_height(px)
            return (px, floor + ARCH_SPRING_HEIGHT + math.sin(angle) * height,
                    math.cos(angle) * radius)

        vertices = [point(x - half_width, a0), point(x - half_width, a1),
                    point(x + half_width, a1), point(x + half_width, a0)]
        middle = (a0 + a1) * 0.5
        append_quad("ArchSeam", vertices, (0.0, -math.sin(middle), -math.cos(middle)))

    # Keep the rib continuous down to the route floor. These uprights are the
    # authored acrylic-frame supports, not the branching volume-mask artifact.
    floor = route_height(x)
    for side in (-1.0, 1.0):
        add_box(
            "ArchSeam",
            (x, floor + ARCH_SPRING_HEIGHT * 0.5, side * radius),
            (half_width * 2.0, ARCH_SPRING_HEIGHT, 0.055),
        )


def build_layout():
    segment_length = 2.0
    for index in range(24):
        x0 = index * segment_length
        x1 = x0 + segment_length
        add_slope_slab(
            "ArchFloor", x0, x1,
            -WALKWAY_HALF_WIDTH, WALKWAY_HALF_WIDTH)

    # A fixed world-space tank surface remains above the descending tunnel.
    # Water depth therefore increases naturally toward the route exit.
    add_water_surface_grid()
    add_refracted_light_curtains()
    add_bubble_plumes()

    # The published reference uses opaque waist rails below a broad acrylic
    # canopy. Avoid stacked transparent side sheets: their blend order reads as
    # a false branching path in perspective.
    add_arch_surface(
        "TankWaterArch", ARCH_WATER_RADIUS, ARCH_WATER_HEIGHT,
        inward=True)
    add_arch_surface(
        "TankGlassArch", ARCH_GLASS_RADIUS, ARCH_GLASS_HEIGHT,
        inward=True)

    # Give the clear water a readable scale reference without filling the
    # exhibit with fish. Low tank beds and sparse rounded reef clusters sit
    # outside the acrylic shell, so they remain visible through refraction but
    # never intrude into the dry visitor route.
    for index in range(12):
        x0 = index * 4.0
        x1 = x0 + 4.0
        add_slope_slab("ArchRock", x0, x1, 3.72, 8.85,
                       thickness=0.34, lift=-0.72)
        add_slope_slab("ArchRock", x0, x1, -8.85, -3.72,
                       thickness=0.34, lift=-0.72)

    reef_clusters = (
        (5.5, -5.2, 1.45, 0.72, 1.05),
        (8.0, -6.4, 0.85, 0.48, 0.72),
        (12.5, 5.5, 1.75, 0.92, 1.22),
        (15.0, 7.0, 0.92, 0.55, 0.82),
        (20.5, -5.8, 1.90, 0.88, 1.30),
        (23.0, -7.2, 1.05, 0.58, 0.90),
        (28.5, 5.1, 1.55, 0.78, 1.16),
        (31.0, 6.8, 0.92, 0.52, 0.76),
        (36.5, -5.4, 1.80, 0.90, 1.25),
        (39.0, -7.0, 0.82, 0.46, 0.68),
        (43.5, 5.7, 1.65, 0.82, 1.14),
        (46.0, 7.1, 0.95, 0.54, 0.80),
    )
    for cluster_index, (x, z, rx, ry, rz) in enumerate(reef_clusters):
        bed_y = route_height(x) - 0.54
        add_ellipsoid(
            "ArchRock",
            (x, bed_y + ry * 0.54, z),
            (rx, ry, rz),
            latitude_segments=7,
            longitude_segments=12,
            seed=float(cluster_index) + 0.37)

    # Thin acrylic seams establish scale without the heavy black cage look.
    for x in range(3, 49, 3):
        add_arch_rib(float(x))

    # Low opaque side rails hide the canopy spring and keep the dry route
    # readable without adding practical lights to the floor.
    rail_profile = (
        (0.00, 3.12),
        (0.12, 3.06),
        (0.82, 3.06),
        (0.99, 3.16),
        (1.02, 3.39),
        (0.90, 3.49),
        (0.10, 3.49),
        (0.00, 3.42),
    )
    trim_profile = (
        (0.96, 3.10),
        (1.02, 3.07),
        (1.10, 3.13),
        (1.12, 3.43),
        (1.05, 3.50),
        (0.98, 3.47),
    )
    for index in range(24):
        x0 = index * 2.0
        x1 = x0 + 2.0
        for side in (-1.0, 1.0):
            add_slope_profile("ArchRail", x0, x1, rail_profile, side)
            add_slope_profile("ArchTrim", x0, x1, trim_profile, side)

    exit_y = route_height(48.0)
    # The old 3.5 m solid end slab looked undersized after raising the arch.
    # Use a five-meter open portal frame aligned to the new crown instead. It
    # stays non-emissive so the water-surface light banks, not the destination,
    # establish the route rhythm.
    for side in (-1.0, 1.0):
        add_box("ArchRail", (48.35, exit_y + 2.45, side * 2.38), (0.35, 4.90, 0.25))
        add_box("ArchTrim", (48.52, exit_y + 2.20, side * 2.18), (0.06, 4.10, 0.055))
    add_box("ArchRail", (48.35, exit_y + 4.88, 0.0), (0.35, 0.24, 5.00))
    add_box("ArchTrim", (48.52, exit_y + 4.24, 0.0), (0.06, 0.055, 4.42))


def pad4(data: bytearray, value=0):
    while len(data) % 4:
        data.append(value)


def write_glb():
    binary = bytearray()
    views, accessors, meshes, nodes, materials = [], [], [], [], []
    material_indices = {}
    for name, rgba in MATERIALS.items():
        material_indices[name] = len(materials)
        materials.append({
            "name": name,
            "pbrMetallicRoughness": {
                "baseColorFactor": list(rgba),
                "metallicFactor": 0.58 if name == "Metal" else 0.02,
                "roughnessFactor": 0.16 if name.startswith("Tank") else 0.72,
            },
            "emissiveFactor": (
                [0.04, 1.1, 2.0]
                if name == "EmissiveCyan"
                else [0, 0, 0]),
            "alphaMode": (
                "BLEND"
                if (name.startswith("Tank") or
                    name in {"ArchWaterSurface", "ArchBubble", "ArchLightCurtain"})
                else "OPAQUE"
            ),
            "doubleSided": (
                name.startswith("Tank") or
                name in {"ArchWaterSurface", "ArchBubble", "ArchLightCurtain"}),
        })

    def view(payload, target):
        pad4(binary)
        offset = len(binary)
        binary.extend(payload)
        views.append({"buffer": 0, "byteOffset": offset,
                      "byteLength": len(payload), "target": target})
        return len(views) - 1

    def accessor(view_index, component, count, kind, minimum=None, maximum=None):
        result = {"bufferView": view_index, "componentType": component,
                  "count": count, "type": kind}
        if minimum is not None:
            result["min"] = minimum
            result["max"] = maximum
        accessors.append(result)
        return len(accessors) - 1

    for name, group in groups.items():
        if not group.indices:
            continue
        points = list(zip(group.positions[0::3], group.positions[1::3], group.positions[2::3]))
        pa = accessor(view(struct.pack(f"<{len(group.positions)}f", *group.positions), 34962),
                      5126, len(points), "VEC3",
                      [min(p[a] for p in points) for a in range(3)],
                      [max(p[a] for p in points) for a in range(3)])
        na = accessor(view(struct.pack(f"<{len(group.normals)}f", *group.normals), 34962),
                      5126, len(points), "VEC3")
        ta = accessor(view(struct.pack(f"<{len(group.texcoords)}f", *group.texcoords), 34962),
                      5126, len(points), "VEC2")
        ia = accessor(view(struct.pack(f"<{len(group.indices)}I", *group.indices), 34963),
                      5125, len(group.indices), "SCALAR")
        meshes.append({"name": name, "primitives": [{
            "attributes": {"POSITION": pa, "NORMAL": na, "TEXCOORD_0": ta},
            "indices": ia, "material": material_indices[name], "mode": 4}]})
        nodes.append({"name": name, "mesh": len(meshes) - 1})

    document = {
        "asset": {"version": "2.0", "generator": "Underwater Arch Generator"},
        "scene": 0,
        "scenes": [{"name": "Descending_Underwater_Arch", "nodes": list(range(len(nodes)))}],
        "nodes": nodes, "meshes": meshes, "materials": materials,
        "accessors": accessors, "bufferViews": views,
        "buffers": [{"byteLength": len(binary)}],
        "extras": {"units": "meters", "routeDirection": "+X",
                   "length": 48.0, "descent": 4.7, "walkWidth": 4.36,
                   "waterSurfaceY": WATER_SURFACE_HEIGHT},
    }
    json_data = bytearray(json.dumps(document, separators=(",", ":")).encode())
    pad4(json_data, 0x20)
    pad4(binary)
    total = 12 + 8 + len(json_data) + 8 + len(binary)
    OUTPUT_GLB.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT_GLB.open("wb") as output:
        output.write(struct.pack("<4sII", b"glTF", 2, total))
        output.write(struct.pack("<I4s", len(json_data), b"JSON"))
        output.write(json_data)
        output.write(struct.pack("<I4s", len(binary), b"BIN\0"))
        output.write(binary)
    return {"meshes": len(meshes), "vertices": sum(len(g.positions) // 3 for g in groups.values()),
            "triangles": sum(len(g.indices) // 3 for g in groups.values()), "bytes": len(binary)}


def create_plan():
    image = Image.new("RGB", (1600, 900), (7, 12, 20))
    draw = ImageDraw.Draw(image)
    draw.text((70, 55), "DESCENDING UNDERWATER ARCH / 48 m", fill=(235, 246, 255))
    draw.text((70, 88), "KEY 4 PREVIEW - water light only - 4.7 m descent", fill=(94, 193, 230))
    # Longitudinal section.
    left, right, base = 110, 1490, 500
    points = []
    for i in range(97):
        x = 48.0 * i / 96
        px = left + (right - left) * x / 48.0
        py = base - route_height(x) * 55.0
        points.append((px, py))
    surface_y = base - WATER_SURFACE_HEIGHT * 55.0
    water_top = [(x, surface_y) for x, _ in points]
    draw.polygon(water_top + list(reversed(points)), fill=(8, 62, 105))
    draw.line(water_top, fill=(45, 197, 244), width=8)
    draw.line(points, fill=(230, 183, 70), width=8)
    for distance in range(0, 49, 6):
        px = left + (right - left) * distance / 48.0
        py = base - route_height(distance) * 55.0
        draw.line((px, py, px, py - 190), fill=(67, 126, 158), width=4)
    draw.text((left, base + 35), "ENTRY  0.0 m", fill=(220, 230, 235))
    draw.text((right - 155, points[-1][1] + 35), "EXIT  -4.7 m", fill=(220, 230, 235))
    draw.text((left + 18, surface_y + 18), "REAL WATER SURFACE  Y = +5.8 m", fill=(80, 215, 245))
    draw.text((110, 800), "Rhythm: cyan entry -> rib shadows -> deep blue exit", fill=(120, 192, 218))
    OUTPUT_PLAN.parent.mkdir(parents=True, exist_ok=True)
    image.save(OUTPUT_PLAN)


def validate():
    data = OUTPUT_GLB.read_bytes()
    magic, version, length = struct.unpack_from("<4sII", data)
    if magic != b"glTF" or version != 2 or length != len(data):
        raise RuntimeError("Invalid GLB")


if __name__ == "__main__":
    build_layout()
    stats = write_glb()
    create_plan()
    validate()
    print(json.dumps({"glb": str(OUTPUT_GLB), "plan": str(OUTPUT_PLAN), **stats}))
