#pragma once

#include "../rendering/AquariumRenderer.h"
#include <DirectXMath.h>

namespace story {

// 管理室奥の大水槽照明コンソール。色の選択は「保存」まで反映せず、
// 保存後だけ短い確認カメラを再生して元の操作位置へ戻す。
class TankLightingConsole {
public:
    enum class Color { Blue=0, White=2 }; // Preserve white's existing save ID.
    enum class Feedback { None, Select, Save };

    void Reset(Color color=Color::Blue);
    void Open(const AquariumSettings& settings);
    void Update(float deltaTime,AquariumSettings& settings);
    void Draw();

    [[nodiscard]] bool Active() const {return phase_!=Phase::Closed;}
    [[nodiscard]] bool WantsCursor() const {return phase_==Phase::Menu;}
    [[nodiscard]] bool Previewing() const {return phase_!=Phase::Closed&&phase_!=Phase::Menu;}
    [[nodiscard]] Color AppliedColor() const {return applied_;}
    Feedback ConsumeFeedback();

private:
    enum class Phase {Closed,Menu,FadeToPreview,PreviewIn,Hold,FadeToReturn,ReturnIn};
    static float Smooth(float value);
    void SetPreviewCamera(AquariumSettings& settings,float dolly=0.f) const;

    Phase phase_=Phase::Closed;
    Color selected_=Color::Blue,applied_=Color::Blue;
    Feedback feedback_=Feedback::None;
    DirectX::XMFLOAT3 savedEye_{};
    float savedYaw_=0,savedPitch_=0,clock_=0,fadeAlpha_=0;
};
}
