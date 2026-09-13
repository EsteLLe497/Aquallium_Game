#pragma once

#include <DirectXMath.h>
#include <filesystem>
#include <fstream>
#include <cmath>

namespace lighting
{
// Runtime-editable palette for the hero tank. Game logic only needs to flip
// alternateEnabled when a switch is used; rendering, caustics, emitters and
// hall bounce all consume the same selected colour.
struct HeroTankLightingRig
{
    DirectX::XMFLOAT3 defaultColor{0.005f, 0.020f, 1.000f};
    DirectX::XMFLOAT3 redColor{1.f,.008f,.012f}; // Legacy config slot; no longer selectable.
    DirectX::XMFLOAT3 whiteColor{1.f,1.f,1.f};
    DirectX::XMFLOAT3 alternateColor{1.f,1.f,1.f};
    // The overhead source is authored close to 5000 K. Water attenuation turns
    // its volume blue while fish near the key retain natural colour rendering.
    DirectX::XMFLOAT3 overheadKeyColor{1.000f, 0.910f, 0.760f};
    float intensity = 1.0f;
    float overheadKeyIntensity = 0.78f;
    float sideLightIntensity = 0.46f;
    bool alternateEnabled = false;
    // Reception tank: offsets from tank centre and water surface (metres).
    DirectX::XMFLOAT3 keyOffset{0.f,1.1f,-1.6f};
    DirectX::XMFLOAT3 keyDirection{0.f,-1.f,.45f};
    DirectX::XMFLOAT3 sideOffset{4.8f,1.1f,.65f};
    DirectX::XMFLOAT3 sideDirection{-.16f,-1.f,.30f};
    float keyConeDegrees=58.f,sideConeDegrees=42.f;
    bool Save(const std::filesystem::path& path) const {
        std::ofstream out(path);
        for(const auto& v:{defaultColor,redColor,whiteColor,keyOffset,keyDirection,sideOffset,sideDirection})
            out<<v.x<<' '<<v.y<<' '<<v.z<<'\n';
        out<<keyConeDegrees<<' '<<sideConeDegrees<<' '<<intensity<<' '
           <<overheadKeyIntensity<<' '<<sideLightIntensity<<'\n';
        return bool(out);
    }
    bool Load(const std::filesystem::path& path) {
        std::ifstream in(path);auto candidate=*this;
        for(auto* v:{&candidate.defaultColor,&candidate.redColor,&candidate.whiteColor,&candidate.keyOffset,
                    &candidate.keyDirection,&candidate.sideOffset,&candidate.sideDirection})
            if(!(in>>v->x>>v->y>>v->z)||!std::isfinite(v->x)||!std::isfinite(v->y)||!std::isfinite(v->z))return false;
        if(!(in>>candidate.keyConeDegrees>>candidate.sideConeDegrees>>candidate.intensity
               >>candidate.overheadKeyIntensity>>candidate.sideLightIntensity))return false;
        if(!(candidate.keyConeDegrees>=10 && candidate.keyConeDegrees<=85 &&
             candidate.sideConeDegrees>=10 && candidate.sideConeDegrees<=85 &&
             candidate.intensity>=0 && candidate.intensity<=2.5f &&
             candidate.overheadKeyIntensity>=0 && candidate.overheadKeyIntensity<=2.5f &&
             candidate.sideLightIntensity>=0 && candidate.sideLightIntensity<=2.f))return false;
        *this=candidate;return true;
    }
};
}
