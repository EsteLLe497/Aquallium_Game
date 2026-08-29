"""Static checks for the route-06 surface-lighting contract."""

from __future__ import annotations

import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GLB_PATH = ROOT / "model" / "aquarium_underwater_arch.glb"


def load_glb_json(path: Path) -> dict:
    data = path.read_bytes()
    magic, version, total_length = struct.unpack_from("<III", data, 0)
    assert magic == 0x46546C67 and version == 2
    assert total_length == len(data)
    chunk_length, chunk_type = struct.unpack_from("<II", data, 12)
    assert chunk_type == 0x4E4F534A
    return json.loads(data[20:20 + chunk_length].decode("utf-8"))


def main() -> None:
    document = load_glb_json(GLB_PATH)
    materials = {material["name"]: material for material in document["materials"]}
    required = {"ArchWaterSurface", "ArchBubble", "ArchLightCurtain"}
    assert required <= materials.keys(), required - materials.keys()
    assert materials["ArchBubble"]["alphaMode"] == "BLEND"
    assert materials["ArchLightCurtain"]["alphaMode"] == "BLEND"
    assert "ArchOverheadEmitter" not in materials

    mesh_names = {mesh["name"] for mesh in document["meshes"]}
    assert required <= mesh_names, required - mesh_names

    stage_shader = (ROOT / "shaders" / "Stage.hlsl").read_text(encoding="utf-8")
    stage_model = (ROOT / "src" / "StageModel.cpp").read_text(encoding="utf-8")
    renderer = (ROOT / "src" / "AquariumRenderer.cpp").read_text(encoding="utf-8")
    for surface_type in ("23.5", "25.5"):
        assert surface_type in stage_shader
    for material_name in ("ArchBubble", "ArchLightCurtain"):
        assert material_name in stage_model
    assert "!settings.greyboxMode" in renderer
    assert "routePositions[lightIndex], 7.40f" in renderer

    primitive_count = sum(len(mesh["primitives"]) for mesh in document["meshes"])
    print(json.dumps({
        "glb": str(GLB_PATH),
        "materials": len(materials),
        "meshes": len(document["meshes"]),
        "primitives": primitive_count,
        "surface_lighting_contract": "ok",
    }, ensure_ascii=False))


if __name__ == "__main__":
    main()
