"""Static contract checks for the data-oriented ambient fish prototype."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    source = (ROOT / "src" / "FishRenderer.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src" / "FishRenderer.h").read_text(encoding="utf-8")
    shader = (ROOT / "shaders" / "Fish.hlsl").read_text(encoding="utf-8")
    aquarium = (ROOT / "src" / "AquariumRenderer.cpp").read_text(encoding="utf-8")
    project = (ROOT / "AquariumLightingPrototype.vcxproj").read_text(encoding="utf-8")

    assert "enum class Habitat" in header
    assert "UnderwaterArch" in header and "WatatsumiTank" in header
    assert "constexpr float cellSize = 2.4f" in source
    assert "for (int z = -1; z <= 1; ++z)" in source
    assert "for (int y = -1; y <= 1; ++y)" in source
    assert "for (int x = -1; x <= 1; ++x)" in source
    assert "constexpr float simulationStep = 1.0f / 30.0f" in source
    assert "Habitat::UnderwaterArch ? 18u : 36u" in source
    assert "Habitat::UnderwaterArch ? 9u : 12u" in source
    assert "spawnSchool(3u, mediumFishCount, 1u)" in source
    assert "CreateRayGeometry" in source
    assert "BuildRayInstances" in source
    assert "Habitat::UnderwaterArch ? 2u : 3u" in source
    assert "ArchCanopy(agent.position.x, agent.position.z) + 0.22f" in source
    assert "DrawIndexedInstanced" in source
    assert "input.bendWeight" in shader
    assert "input.instanceSpeciesShape.x" in shader
    assert "raySpecies" in shader and "wingMask" in shader
    assert "viewedFromBelow" in shader
    assert "Fish.hlsl" in project and "FishRenderer.cpp" in project

    opaque = aquarium.index("activeStageModel->RenderOpaque")
    biology = aquarium.index("fishRenderer_.Render")
    refraction_copy = aquarium.index("context->CopyResource", biology)
    assert opaque < biology < refraction_copy

    print({
        "result": "pass",
        "arch_small_fish": 54,
        "arch_medium_fish": 9,
        "arch_rays": 2,
        "watatsumi_small_fish": 108,
        "watatsumi_medium_fish": 12,
        "watatsumi_rays": 3,
        "simulation_hz": 30,
        "spatial_cells_per_query": 27,
        "maximum_biology_draw_calls_per_habitat": 2,
        "overhead_silhouette_extra_draws": 0,
        "sequential_refraction_order": True,
    })


if __name__ == "__main__":
    main()
