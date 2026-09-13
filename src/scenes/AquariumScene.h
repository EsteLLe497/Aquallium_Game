/*==================================================================================================

   [AquariumScene.h]
                                                         Author :Masatora Tanaka
                                                         Date   :2026/07/28
----------------------------------------------------------------------------------------------------
   アクアリウムの操作、カメラ、ライティング調整値を所有するシーン
===================================================================================================*/
#pragma once
#include "../audio/BackgroundMusic.h"
#include "../player/TerraceDoor.h"
#include "../player/MiniMap.h"
#include "../player/GameMenu.h"
#include "../player/SaveSystem.h"
#include "../story/OpeningFlow.h"
#include "../story/HeroineEncounter.h"
#include "../story/DialoguePlayer.h"
#include "../story/PasswordLock.h"
#include "../story/TerraceConversation.h"
#include "../story/ArchChase.h"
#include "../story/PowerOutage.h"
#include "../story/TankLightingConsole.h"
#include "../story/StoryTexture.h"
#include "../story/StoryFlowEditor.h"
#include "../audio/SoundEffects.h"
#include "../rendering/ScreenFade.h"

#include "../rendering/AquariumRenderer.h"
#include "../rendering/BeachPreviewRenderer.h"
#include "../rendering/ExitPortalRenderer.h"
#include "../framework/scene.h"
#include "../physics/CollisionWorld.h"
#include "../player/PlayerManager.h"

#include <filesystem>

namespace framework
{
class InputSystem;
}

class AquariumScene final : public framework::Scene
{
public:
    void SetEditorOpen(bool open) { editorOpen_=open; }
    bool WantsCursor() const {return storyFlowEditor_.Visible()||transitionFade_.Active()||gameMenu_.IsOpen()||passwordLock_.Active()||facilityPasswordLock_.Active()||
        terraceConversation_.WantsCursor()||powerOutage_.WantsCursor()||
        tankLightingConsole_.WantsCursor()||beachChoiceActive_;}
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
    [[nodiscard]] lighting::HeroTankLightingRig& GetHeroTankLighting() noexcept
    {
        return settings_.heroTankLighting;
    }

private:
    enum class BeachBranch {Undecided,Stay,Leave};
    void ResetSettings();
    void SelectUnderwaterView();
    void SelectStageGlassView();
    void SelectAquariumGreyboxView();
    void SelectUnderwaterArchView();
    void SelectJellyfishReverseValidationView();
    void SelectWatatsumiTankView();
    void SelectContinuousAquariumView();
    void SelectGameLayoutV3View();
    void SelectReceptionLobbyView();
    void ApplyReceptionHallLighting();
    void UpdatePlayer(float deltaTime, const framework::InputSystem& input);
    void UpdateLightingTuning(float deltaTime, const framework::InputSystem& input);
    void BuildStageGlassCollision();
    void BuildRouteCollision();
    void BuildUnderwaterArchCollision();
    void BuildWatatsumiCollision();
    void BuildContinuousCollision();
    void BuildGameLayoutV3Collision();
    void BuildReceptionLobbyCollision();
    void ResetPlayer();
    void StartNewGame();
    player::SaveData CaptureSave() const;
    void ApplySave(const player::SaveData& data);
    void ProcessMenuRequest();
    void BeginSceneTransition(player::GameMenu::Request request);
    bool UpdateSceneTransition(float deltaTime);
    void ApplySceneTransition();
    void DrawSceneFade() const;
    void DrawInWaterStill() const;
    void FinishInWaterIntro();
    void ToggleBeachPreview(bool morning);
    void BeginEmergencyExitTransition();
    bool UpdateEmergencyExitTransition(float deltaTime);
    void BeginMorningBeachTransition();
    bool UpdateMorningBeachTransition(float deltaTime);
    void UpdateMorningWake(float deltaTime);
    void UpdateMorningStand(float deltaTime);
    void StartMorningAmbience();
    void UpdateMorningAmbience(float deltaTime);
    void BeginMorningAmbienceFadeOut();
    void StopMorningAmbience();
    void EnterStoryBeach();
    void BuildBeachCollision();
    void DrawBeachChoice();
    void ApplyHeroTankLightColor(story::TankLightingConsole::Color color);

    AquariumRenderer renderer_;
    BeachPreviewRenderer beachRenderer_;
    ExitPortalRenderer exitPortalRenderer_;
    AquariumSettings settings_;
    float simulationTime_ = 0.0f;
    physics::CollisionWorld stageGlassCollision_;
    physics::CollisionWorld routeCollision_;
    physics::CollisionWorld underwaterArchCollision_;
    physics::CollisionWorld watatsumiCollision_;
    physics::CollisionWorld continuousCollision_;
    physics::CollisionWorld gameLayoutV3Collision_;
    physics::CollisionWorld receptionLobbyCollision_;
    physics::CollisionWorld beachCollision_;
    player::PlayerManager playerManager_;
    player::PlayerManager beachPlayer_;
    player::TerraceDoor terraceDoor_;
    story::OpeningFlow opening_;
    story::HeroineEncounter heroineEncounter_;
    story::DialoguePlayer eventDialogue_;
    story::PasswordLock passwordLock_;
    story::PasswordLock facilityPasswordLock_;
    story::TerraceConversation terraceConversation_;
    story::ArchChase archChase_;
    story::PowerOutage powerOutage_;
    story::TankLightingConsole tankLightingConsole_;
    player::GameMenu gameMenu_;
    player::SaveSystem saveSystem_;
    player::MiniMap miniMap_;
    audio::SoundEffects soundEffects_;
    audio::BackgroundMusic backgroundMusic_;
    rendering::ScreenFade transitionFade_;
    story::StoryTexture inWaterStill_;
    story::StoryFlowEditor storyFlowEditor_;
    player::GameMenu::Request pendingTransition_{};
    std::filesystem::path storyFolder_;
    std::filesystem::path beachSoundPath_;
    bool clueCollected_=false,managementCollisionOpened_=false,heroineJoined_=false;
    bool managementDoorTargetOpen_=false;
    float managementDoorAngle_=0.f;
    bool staffDoorTargetOpen_=false,staffDoorCollisionOpened_=false;
    float staffDoorAngle_=0.f;
    float playTimeSeconds_=0.f;
    bool editorOpen_=false;
    bool facilityBootDialoguePending_=false;
    bool beachPreview_=false;
    bool beachMorning_=false;
    bool beachStoryMode_=false;
    bool beachArrivalLook_=false;
    float beachArrivalLookTime_=0.f;
    bool beachArrivalDialoguePendingSit_=false;
    bool beachSitTransition_=false;
    bool beachSeated_=false;
    bool beachSeatedDialoguePendingChoice_=false;
    bool beachChoiceActive_=false;
    BeachBranch beachBranch_=BeachBranch::Undecided;
    bool beachLeaveDialoguePendingTransition_=false;
    bool morningBeachTransition_=false;
    bool morningBeachEntered_=false;
    float morningBeachTransitionTime_=0.f;
    bool morningWake_=false;
    float morningWakeTime_=0.f;
    float morningWakeBlur_=0.f;
    float morningWakeBlink_=0.f;
    bool morningWakeDialoguePendingStand_=false;
    bool morningStandTransition_=false;
    float morningStandTime_=0.f;
    bool morningAmbienceFading_=false;
    float morningAmbienceFadeTime_=0.f;
    float morningAmbienceAppliedVolume_=-1.f;
    bool morningAmbienceFadingOut_=false;
    float morningAmbienceFadeOutTime_=0.f;
    float morningAmbienceFadeOutStartVolume_=0.f;
    float beachSitTransitionTime_=0.f;
    DirectX::XMFLOAT3 beachSitStartEye_{};
    float beachSitStartYaw_=0.f,beachSitStartPitch_=0.f;
    bool emergencyExitTransition_=false;
    bool emergencyExitBeachEntered_=false;
    float emergencyExitTransitionTime_=0.f;
    bool inWaterIntro_=false;
    bool inWaterIntroDialoguePending_=false;
    bool inWaterIntroTransition_=false;
    DirectX::XMFLOAT3 beachReturnEye_{};
    float beachReturnYaw_=0.f,beachReturnPitch_=0.f;
};
