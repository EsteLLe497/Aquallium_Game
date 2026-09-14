#pragma once

#include "../rendering/AquariumRenderer.h"
#include <DirectXMath.h>

namespace story {

// 展示室の手掛かり取得後、地下から1Fへ戻る途中で発生する水中アーチ演出。
// 台詞・SE要求を外へ通知し、レンダラーへは0..1の軽量な演出値だけを渡す。
class ArchChase {
public:
  enum class Request {
    None,
    StagnationDialogue,
    Creak,
    CreakDialogue,
    PredatorAppear,
    EscapeDialogue,
    AftermathDialogue,
    Impact
  };

  void Reset();
  void BeginExtendedForQa(AquariumSettings& settings) {
    extension_=30;settings.archExtension=extension_;
    BeginRevealForQa(settings);
  }
  void MarkComplete() { phase_ = Phase::Complete; }
  void BeginRevealForQa(const AquariumSettings &settings) {
    phase_ = Phase::Reveal;
    clock_ = 0;
    anchor_ = {settings.cameraPositionX, settings.cameraPositionY,
               settings.cameraPositionZ};
    anchorYaw_ = settings.cameraYaw;
    anchorPitch_ = settings.cameraPitch;
    havePreviousZ_ = true;
    previousZ_ = settings.cameraPositionZ;
  }
  void Update(float deltaTime, bool eligible, bool movingTowardEntrance,
              bool dialogueActive, AquariumSettings &settings);
  void Draw() const;
  [[nodiscard]] bool BlocksPlayer() const;
  [[nodiscard]] bool ConstrainsPlayer() const;
  [[nodiscard]] bool UsesExtendedPath() const { return phase_ == Phase::Stagnation || extension_ > 0.f; }
  void CollapseExtension() { extension_=0; }
  [[nodiscard]] bool Complete() const;
  [[nodiscard]] bool Escaped() const {
    return phase_ == Phase::Recover || phase_ == Phase::WaitAftermath ||
           phase_ == Phase::Complete;
  }
  [[nodiscard]] bool Active() const;
  [[nodiscard]] bool PlayerCanRun() const { return phase_ == Phase::Chase; }
  [[nodiscard]] bool GameOver() const { return phase_ == Phase::GameOver; }
  [[nodiscard]] Request ConsumeRequest();

private:
  enum class Phase {
    Dormant,
    Stagnation,
    WaitStagnation,
    Creak,
    WaitCreak,
    Reveal,
    WaitEscape,
    Chase,
    Impact,
    Devour,
    GameOver,
    Recover,
    WaitAftermath,
    Complete
  };
  void BeginDialogueWait(Phase phase, Request request);
  void ApplyCamera(AquariumSettings &settings, float amount);

  Phase phase_ = Phase::Dormant;
  Request request_ = Request::None;
  DirectX::XMFLOAT3 anchor_{};
  float anchorYaw_ = 0, anchorPitch_ = 0, previousZ_ = 0, clock_ = 0;
  float distanceWalked_ = 0, extension_ = 0, timeLimit_ = 10.f;
  float failureFade_ = 0;
  bool havePreviousZ_ = false, dialogueObserved_ = false;
};
} // namespace story
