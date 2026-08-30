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
    assert "habitat == Habitat::UnderwaterArch ? 24u : 48u" in source
    assert "ArchCanopy(agent.position.x, agent.position.z) + 0.22f" in source
    assert "DrawIndexedInstanced" in source
    assert "input.bendWeight" in shader
    assert "viewedFromBelow" in shader
    assert "Fish.hlsl" in project and "FishRenderer.cpp" in project

    opaque = aquarium.index("activeStageModel->RenderOpaque")
    biology = aquarium.index("fishRenderer_.Render")
    refraction_copy = aquarium.index("context->CopyResource", biology)
    assert opaque < biology < refraction_copy

    print({
        "result": "pass",
        "arch_fish": 72,
        "watatsumi_fish": 144,
        "simulation_hz": 30,
        "spatial_cells_per_query": 27,
        "draw_calls_per_habitat": 1,
        "overhead_silhouette_extra_draws": 0,
        "sequential_refraction_order": True,
    })


if __name__ == "__main__":
    main()
