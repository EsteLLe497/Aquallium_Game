/*==================================================================================================

   [AquariumScene.h]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/07/28
----------------------------------------------------------------------------------------------------
   アクアリウムの操作、カメラ、ライティング調整値を所有するシーン
===================================================================================================*/
#pragma once

#include "../AquariumRenderer.h"
#include "../framework/scene.h"

#include <filesystem>

namespace framework
{
class InputSystem;
}

class AquariumScene final : public framework::Scene
{
public:
    AquariumScene(
        ID3D11Device* device,
        const std::filesystem::path& shaderPath);

    void Update(
        const framework::FrameContext& frame,
        const framework::InputSystem& input) override;
    void Render(const framework::RenderContext& context) override;
    [[nodiscard]] framework::SceneDiagnostics GetDiagnostics() const override;
    [[nodiscard]] lighting::LocalLightingRig& GetLocalLighting() noexcept
    {
        return settings_.localLighting;
    }

private:
    void ResetSettings();
    void SelectUnderwaterView();
    void SelectStageGlassView();
    void SelectAquariumGreyboxView();
    void SelectUnderwaterArchView();
    void SelectJellyfishReverseValidationView();
    void SelectWatatsumiTankView();
    void UpdateCamera(float deltaTime, const framework::InputSystem& input);
    void UpdateLightingTuning(float deltaTime, const framework::InputSystem& input);

    AquariumRenderer renderer_;
    AquariumSettings settings_;
    float simulationTime_ = 0.0f;
    bool watatsumiRampTracking_ = false;
    float watatsumiRampT_ = 0.0f;
    float watatsumiRampMaximumT_ = 0.0f;
};
