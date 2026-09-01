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
    float intensity = 1.0f;
    bool alternateEnabled = false;
};
}
