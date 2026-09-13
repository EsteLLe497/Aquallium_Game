#pragma once
#include <algorithm>
namespace story::arch {
// Shared with Stage.hlsl: insert repeatable, level spans at this section.
inline constexpr float seam=57.f, entrance=28.4f, tileLength=12.f;
// Returning from B1 (decreasing Z): engage 6 m before the repeated seam.
inline constexpr float loopTrigger=seam+6.f;
inline float Floor(float z) {
    const float t=std::clamp((z-25.f)/48.f,0.f,1.f);
    return -2.25f-4.7f*t*t*(3.f-2.f*t);
}
inline float SourceZ(float z,float extension) {
    if(z>=seam)return z;
    if(z<=seam-extension)return z+extension;
    return seam;
}
inline float Floor(float z,float extension){return Floor(SourceZ(z,extension));}
inline float EscapeSeconds(float z,float extension,float speed){
    return std::max(0.f,z-(entrance-extension))/speed+1.5f;
}
}
