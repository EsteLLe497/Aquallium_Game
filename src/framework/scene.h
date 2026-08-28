/*==================================================================================================

   [scene.h]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/06/23
----------------------------------------------------------------------------------------------------

===================================================================================================*/
#pragma once

#include "FrameContext.h"
#include "RenderContext.h"

#include <string>

namespace framework
{
class InputSystem;

struct SceneDiagnostics
{
    std::wstring viewLabel = L"SCENE";
    float causticsStrength = 0.0f;
    float volumeStrength = 0.0f;
    float anisotropy = 0.0f;
    float exposure = 0.0f;
    bool paused = false;
};

class Scene
{
public:
    virtual ~Scene() = default;

    virtual void Init() {}
    virtual void Uninit() {}
    virtual void OnEnter() { Init(); }
    virtual void OnExit() { Uninit(); }
    virtual void Update(const FrameContext& frame, const InputSystem& input) = 0;
    virtual void Render(const RenderContext& context) = 0;
    [[nodiscard]] virtual SceneDiagnostics GetDiagnostics() const = 0;
};
}

// DM31_Gameのグローバル型名との互換
using Scene = framework::Scene;
