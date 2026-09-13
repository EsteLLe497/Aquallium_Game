#pragma once

#include "../rendering/AquariumRenderer.h"
#include <DirectXMath.h>

namespace story {

// テラスのベンチ専用フロー。会話本文はDialoguePlayerへ任せ、
// ここではチュートリアル、着座姿勢、選択肢、立ち上がりだけを扱う。
class TerraceConversation {
public:
    enum class Request { None, Gossip, Hint, RestDialogue, RestCompleted };

    void Reset();
    void UnlockTutorial();
    void RestoreAfterEncounter(){tutorialPending_=false;tutorialSeen_=true;phase_=Phase::Free;}
    void StartSitting(const DirectX::XMFLOAT3& eye,float yaw,float pitch,
                      bool playMissionDialogue=false);
    void Update(float deltaTime,bool dialogueActive,AquariumSettings& settings);
    void Draw(bool hintAvailable);

    [[nodiscard]] bool BlocksPlayer() const;
    [[nodiscard]] bool WantsCursor() const;
    [[nodiscard]] bool CanSit() const;
    [[nodiscard]] Request ConsumeRequest();

private:
    enum class Phase { Free, Sitting, Menu, WaitingDialogue, Standing };
    static float Smooth(float value);
    void ApplyTransition(AquariumSettings& settings,float progress,bool sitting) const;

    Phase phase_=Phase::Free;
    Request request_=Request::None;
    DirectX::XMFLOAT3 startEye_{};
    float startYaw_=0.f,startPitch_=0.f,clock_=0.f;
    bool tutorialPending_=false,tutorialSeen_=false,dialogueObserved_=false;
    bool restDialogue_=false,missionDialoguePending_=false;
};
}
