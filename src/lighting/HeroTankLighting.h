#pragma once

#include <DirectXMath.h>

namespace lighting
{
// Runtime-editable palette for the hero tank. Game logic only needs to flip
// alternateEnabled when a switch is used; rendering, caustics, emitters and
// hall bounce all consume the same selected colour.
struct HeroTankLightingRig
{
    DirectX::XMFLOAT3 defaultColor{0.035f, 0.390f, 1.000f};
    DirectX::XMFLOAT3 alternateColor{0.950f, 0.075f, 0.280f};
    // A mostly neutral key light separates foreground fish from the blue
    // water. The two small inward-facing side sources retain the puzzle colour.
    DirectX::XMFLOAT3 frontKeyColor{0.620f, 0.840f, 1.000f};
    float intensity = 1.0f;
    float frontKeyIntensity = 0.86f;
    float sideLightIntensity = 0.46f;
    bool alternateEnabled = false;
};
}
