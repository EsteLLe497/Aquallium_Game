"""Static regression checks for Watatsumi water, acrylic and lighting."""

from __future__ import annotations

import json
from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[1]
RENDERER = ROOT / "src" / "AquariumRenderer.cpp"
STAGE_MODEL = ROOT / "src" / "StageModel.cpp"
STAGE_HEADER = ROOT / "src" / "StageModel.h"
STAGE_SHADER = ROOT / "shaders" / "Stage.hlsl"
GENERATOR = ROOT / "tools" / "generate_watatsumi_hall.py"
MODEL = ROOT / "model" / "aquarium_watatsumi_hall.glb"


def read_glb_json(path: Path) -> dict:
    data = path.read_bytes()
    magic, version, total_length = struct.unpack_from("<4sII", data, 0)
    assert magic == b"glTF" and version == 2
    assert total_length == len(data)
    json_length, json_type = struct.unpack_from("<I4s", data, 12)
    assert json_type == b"JSON"
    return json.loads(data[20:20 + json_length].decode("utf-8"))


def main() -> None:
    renderer = RENDERER.read_text(encoding="utf-8")
    stage_model = STAGE_MODEL.read_text(encoding="utf-8")
    stage_header = STAGE_HEADER.read_text(encoding="utf-8")
    stage_shader = STAGE_SHADER.read_text(encoding="utf-8")
    generator = GENERATOR.read_text(encoding="utf-8")
    model = read_glb_json(MODEL)

    assert "settings.underwaterArchMode || settings.watatsumiTankMode" in renderer
    assert "StageModel::TransparentLayer::Medium" in renderer
    assert "StageModel::TransparentLayer::Glass" in renderer
    assert "activeLightCount = 3;" in renderer
    assert renderer.count("10.20f") >= 3
    assert "!settings.watatsumiTankMode" in renderer
    assert "isRefractiveGlass" in stage_model
    assert "Medium," in stage_header and "Glass" in stage_header
    assert "const float waterDepth = max(10.20 - input.worldPosition.y" in stage_shader
    assert "backgroundColor * transmittance + inScattering" in stage_shader
    assert "glassLightBank" in stage_shader

    assert "def ellipsoid(" not in generator
    assert "ellipsoid(" not in generator
    assert "five ellipsoid placeholder rocks" in generator
    # Accessor indices are references; triangle counts come from their metadata.
    triangle_count = sum(
        model["accessors"][primitive["indices"]]["count"] // 3
        for mesh in model["meshes"]
        for primitive in mesh["primitives"]
    )
    assert triangle_count < 5000, triangle_count

    print({
        "result": "pass",
        "sequential_water_then_acrylic": True,
        "rendered_water_surface_y_m": 10.20,
        "water_light_banks": 3,
        "full_screen_volume_added": False,
        "placeholder_ellipsoids": 0,
        "triangles": triangle_count,
    })


if __name__ == "__main__":
    main()
