"""Static checks for the route-07 chunk boundaries and integration contract."""

from __future__ import annotations

import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read_glb(path: Path) -> dict:
    data = path.read_bytes()
    magic, version, length = struct.unpack_from("<4sII", data, 0)
    assert magic == b"glTF" and version == 2 and length == len(data)
    json_length, json_kind = struct.unpack_from("<I4s", data, 12)
    assert json_kind == b"JSON"
    return json.loads(data[20:20 + json_length].decode("utf-8").rstrip(" "))


def main() -> None:
    shell = read_glb(ROOT / "model" / "aquarium_continuous_shell.glb")
    hall = read_glb(ROOT / "model" / "aquarium_watatsumi_hall.glb")
    names = {material["name"] for material in shell["materials"]}
    assert {"Floor", "DarkWall", "TankWaterDisplayBox",
            "TankGlassJellyCylinder"} <= names
    assert hall["extras"]["inferredTankSize"][0] == 10.5

    renderer = (ROOT / "src" / "AquariumRenderer.cpp").read_text(encoding="utf-8")
    scene = (ROOT / "src" / "scenes" / "AquariumScene.cpp").read_text(
        encoding="utf-8")
    project = (ROOT / "AquariumLightingPrototype.vcxproj").read_text(
        encoding="utf-8")
    assert "continuousArchImport.translation = {22.0f, 0.0f, 17.80f}" in renderer
    assert "Continuous_DescendingUnderwaterArch" in scene
    assert "Continuous_EntranceFloor" in scene
    assert "Continuous_PanoramaJellyRoom" in scene
    assert "aquarium_continuous_shell.glb" in project
    assert "case L'7': SelectContinuousAquariumView(); break;" in scene

    print(json.dumps({
        "route": 7,
        "shellMeshes": len(shell["meshes"]),
        "hallDepthMeters": hall["extras"]["inferredTankSize"][0],
        "continuousCollision": True,
        "previewKey": 7,
    }))


if __name__ == "__main__":
    main()
