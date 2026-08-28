#pragma once

#include <DirectXMath.h>
#include <array>
#include <cstdint>

namespace lighting
{
constexpr std::uint32_t kMaximumLocalLights = 8;

enum class LocalLightType : std::uint32_t
{
    Point = 0,
    Spot = 1
};

struct LocalLight
{
    DirectX::XMFLOAT3 position{};
    float range = 7.0f;
    DirectX::XMFLOAT3 direction{0.0f, -1.0f, 0.0f};
    float intensity = 3.0f;
    DirectX::XMFLOAT3 color{0.30f, 0.70f, 1.0f};
    float innerConeDegrees = 24.0f;
    float outerConeDegrees = 38.0f;
    LocalLightType type = LocalLightType::Spot;
    bool enabled = true;
};

struct LocalLightingRig
{
    std::array<LocalLight, kMaximumLocalLights> lights{};
    std::uint32_t lightCount = 0;
    DirectX::XMFLOAT3 ambientColor{0.030f, 0.075f, 0.115f};
    float ambientStrength = 0.045f;

    // Broad, shadow-free indirect light emitted by the public tank window.
    // This is intentionally separate from the underwater direct-light rig.
    DirectX::XMFLOAT3 tankBounceCenter{6.90f, 3.35f, 0.0f};
    float tankBounceRange = 18.0f;
    DirectX::XMFLOAT3 tankBounceNormal{-1.0f, 0.0f, 0.0f};
    float tankBounceHalfWidth = 7.35f;
    DirectX::XMFLOAT3 tankBounceColor{0.025f, 0.23f, 0.52f};
    float tankBounceIntensity = 0.34f;
    float tankBounceHalfHeight = 3.05f;

    DirectX::XMFLOAT3 atmosphereColor{0.006f, 0.030f, 0.060f};
    float atmosphereDensity = 0.020f;
    float atmosphereStart = 8.0f;
    float atmosphereMaximum = 0.32f;
    bool enabled = true;
    bool tankBounceEnabled = true;
    bool atmosphereEnabled = true;
};
}
