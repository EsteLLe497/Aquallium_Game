"""Static production checks for the Route 09 jellyfish basement."""

from __future__ import annotations

import json
import math
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "artifacts" / "jelly_basement_manifest.json"
GLB = ROOT / "asset" / "model" / "aquarium_jelly_basement.glb"
HEADER = ROOT / "src" / "generated" / "JellyBasementGenerated.h"


def main() -> None:
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    column = data["column_room"]
    panorama = data["panorama_room"]
    assert column == {
        "width": 16.0, "depth": 14.0, "height": 5.5,
        "tank_ids": [f"JB_TANK_{index:02d}" for index in range(1, 6)]}
    assert panorama["arc_degrees"] == 180
    assert panorama["glass_height"] >= 3.3
    assert len(data["floors"]) == 2
    assert data["floors"][0]["maximum_z"] == data["floors"][1]["minimum_z"]
    assert all(floor["floor_y"] == -6.95 for floor in data["floors"])
    assert len(data["circles"]) == 42  # 5 columns + 37 curved-wall samples
    assert len(data["boxes"]) == 14
    statistics = data["mesh_statistics"]
    assert statistics["triangles"] <= 4500
    assert statistics["vertices"] <= 5200

    tank_centres = ((-4.25, 4.2), (4.25, 4.2), (0.0, 8.4),
                    (-4.25, 12.0), (4.25, 12.0))
    minimum_spacing = min(
        math.dist(first, second)
        for index, first in enumerate(tank_centres)
        for second in tank_centres[index + 1:])
    assert minimum_spacing >= 4.2

    raw = GLB.read_bytes()
    magic, version, length = struct.unpack_from("<III", raw)
    assert magic == 0x46546C67 and version == 2 and length == len(raw)
    header = HEADER.read_text(encoding="utf-8")
    assert "kCircles" in header and "kFloors" in header
    print(json.dumps({
        "status": "PASS", "minimum_tank_spacing": minimum_spacing,
        "triangles": statistics["triangles"],
        "vertices": statistics["vertices"],
        "collision_primitives": len(data["boxes"]) + len(data["circles"]),
    }))


if __name__ == "__main__":
    main()
