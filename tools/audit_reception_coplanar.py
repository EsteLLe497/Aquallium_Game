"""Report coplanar horizontal surfaces in the generated reception model."""

from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import generate_reception_lobby as lobby  # noqa: E402


@dataclass(frozen=True)
class Surface:
    name: str
    material: str
    y: float
    min_x: float
    max_x: float
    min_z: float
    max_z: float


def overlap_area(first: Surface, second: Surface) -> float:
    width = min(first.max_x, second.max_x) - max(first.min_x, second.min_x)
    depth = min(first.max_z, second.max_z) - max(first.min_z, second.min_z)
    return max(width, 0.0) * max(depth, 0.0)


def run() -> list[tuple[float, Surface, Surface]]:
    surfaces: list[Surface] = []
    original_add_box = lobby.route.add_box
    original_append_quad = lobby.append_quad

    def track_box(material, name, center, size):
        cx, cy, cz = center
        sx, sy, sz = size
        for suffix, y in (("top", cy + sy * 0.5),
                          ("bottom", cy - sy * 0.5)):
            surfaces.append(Surface(
                f"{name}:{suffix}", material, y,
                cx - sx * 0.5, cx + sx * 0.5,
                cz - sz * 0.5, cz + sz * 0.5))
        return original_add_box(material, name, center, size)

    def track_quad(material, vertices, normal):
        ys = [vertex[1] for vertex in vertices]
        if max(ys) - min(ys) < 0.00001:
            xs = [vertex[0] for vertex in vertices]
            zs = [vertex[2] for vertex in vertices]
            surfaces.append(Surface(
                f"quad-{len(surfaces)}", material, ys[0],
                min(xs), max(xs), min(zs), max(zs)))
        return original_append_quad(material, vertices, normal)

    lobby.route.add_box = track_box
    lobby.append_quad = track_quad
    try:
        lobby.build()
    finally:
        lobby.route.add_box = original_add_box
        lobby.append_quad = original_append_quad

    conflicts = []
    for index, first in enumerate(surfaces):
        for second in surfaces[index + 1:]:
            if abs(first.y - second.y) > 0.00001:
                continue
            area = overlap_area(first, second)
            if area > 0.01:
                conflicts.append((area, first, second))
    return sorted(conflicts, reverse=True, key=lambda item: item[0])


if __name__ == "__main__":
    for area, first, second in run():
        if (5.30 <= first.y <= 5.70 and
                first.name.endswith(":top") and
                second.name.endswith(":top")):
            print(f"area={area:.3f} y={first.y:.3f} "
                  f"{first.name} [{first.material}] <> "
                  f"{second.name} [{second.material}]")
