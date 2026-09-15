#include "TerraceConversation.h"
#include "../ui/AquariumUi.h"
#include "../../third_party/imgui/imgui.h"
#include <algorithm>
#include <cmath>

namespace story {
namespace {
constexpr DirectX::XMFLOAT3 kSeatedEye{-3.0f, 5.03f, -5.03f};
constexpr DirectX::XMFLOAT3 kStandingEye{-3.0f, 5.14f, -5.78f};
constexpr float kFacingOutside = 3.14159265f;
constexpr float kSitDuration = 1.72f;
constexpr float kStandDuration = 1.58f;
float Mix(float a, float b, float t) { return a + (b - a) * t; }
float MixAngle(float a, float b, float t) {
  return a + std::remainder(b - a, 3.14159265f * 2.f) * t;
}
} // namespace

void TerraceConversation::Reset() {
  phase_ = Phase::Free;
  request_ = Request::None;
  clock_ = 0;
  dialogueObserved_ = false;
  restDialogue_ = missionDialoguePending_ = false;
  tutorialPending_ = tutorialSeen_ = false;
}
void TerraceConversation::UnlockTutorial() {
  if (!tutorialSeen_)
    tutorialPending_ = true;
}
void TerraceConversation::StartSitting(const DirectX::XMFLOAT3 &eye, float yaw,
                                       float pitch,bool playMissionDialogue) {
  if (!CanSit())
    return;
  startEye_ = eye;
  startYaw_ = yaw;
  startPitch_ = pitch;
  clock_ = 0;
  missionDialoguePending_ = playMissionDialogue;
  phase_ = Phase::Sitting;
}
bool TerraceConversation::BlocksPlayer() const {
  return tutorialPending_ || phase_ != Phase::Free;
}
bool TerraceConversation::WantsCursor() const {
  return tutorialPending_ || phase_ == Phase::Menu;
}
bool TerraceConversation::CanSit() const {
  return !tutorialPending_ && phase_ == Phase::Free;
}
TerraceConversation::Request TerraceConversation::ConsumeRequest() {
  const Request value = request_;
  request_ = Request::None;
  return value;
}
float TerraceConversation::Smooth(float value) {
  value = std::clamp(value, 0.f, 1.f);
  return value * value * value * (value * (value * 6.f - 15.f) + 10.f);
}
void TerraceConversation::ApplyTransition(AquariumSettings &settings,
                                          float progress, bool sitting) const {
  const float t = Smooth(progress);
  const auto from = sitting ? startEye_ : kSeatedEye;
  const auto to = sitting ? kSeatedEye : kStandingEye;
  settings.cameraPositionX = Mix(from.x, to.x, t);
  settings.cameraPositionY =
      Mix(from.y, to.y, t) - std::sin(progress * 3.14159265f) * .075f;
  settings.cameraPositionZ = Mix(from.z, to.z, t);
  const float fromYaw = sitting ? startYaw_ : kFacingOutside;
  // Always take the short arc. A seated interaction must never spin the
  // player through the long side of +/-pi just because yaw wrapped there.
  settings.cameraYaw = MixAngle(fromYaw, kFacingOutside, t);
  const float basePitch = sitting ? startPitch_ : 0.f;
  // 腰を下ろす途中だけ自然に足元へ視線が落ち、着座時は水平へ戻る。
  settings.cameraPitch =
      Mix(basePitch, 0.f, t) - std::sin(progress * 3.14159265f) * .105f;
}
void TerraceConversation::Update(float deltaTime, bool dialogueActive,
                                 AquariumSettings &settings) {
  if (phase_ == Phase::Sitting) {
    clock_ += deltaTime;
    const float t = std::min(clock_ / kSitDuration, 1.f);
    ApplyTransition(settings, t, true);
    if (t >= 1.f) {
      clock_ = 0;
      if (missionDialoguePending_) {
        phase_ = Phase::WaitingDialogue;
        dialogueObserved_ = false;
        restDialogue_ = true;
        missionDialoguePending_ = false;
        request_ = Request::RestDialogue;
      } else {
        phase_ = Phase::Menu;
      }
    }
  } else if (phase_ == Phase::Standing) {
    clock_ += deltaTime;
    const float t = std::min(clock_ / kStandDuration, 1.f);
    ApplyTransition(settings, t, false);
    if (t >= 1.f) {
      phase_ = Phase::Free;
      clock_ = 0;
    }
  } else if (phase_ == Phase::Menu || phase_ == Phase::WaitingDialogue) {
    settings.cameraPositionX = kSeatedEye.x;
    settings.cameraPositionY = kSeatedEye.y;
    settings.cameraPositionZ = kSeatedEye.z;
    settings.cameraYaw = kFacingOutside;
    settings.cameraPitch = 0;
    if (phase_ == Phase::WaitingDialogue) {
      dialogueObserved_ |= dialogueActive;
      if (dialogueObserved_ && !dialogueActive) {
        phase_ = Phase::Menu;
        dialogueObserved_ = false;
        if (restDialogue_) {
          restDialogue_ = false;
          request_ = Request::RestCompleted;
        }
      }
    }
  }
}
void TerraceConversation::Draw(bool hintAvailable) {
  (void)hintAvailable;
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  if (tutorialPending_) {
    aquariumUi::drawBackdrop(screen,150);
    aquariumUi::PanelStyle style;
    const float width = std::min(640.f, screen.x * .78f);
    ImGui::SetNextWindowPos({screen.x * .5f, screen.y * .48f}, ImGuiCond_Always,
                            {.5f, .5f});
    ImGui::SetNextWindowSize({width, 350.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {34, 27});
    ImGui::Begin("##terrace_tutorial", nullptr,
                 ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_NoSavedSettings);
    aquariumUi::drawPanelAccent();
    ImGui::TextColored({.55f, .86f, 1.f, 1.f}, "TERRACE  /  会話について");
    ImGui::Dummy({0, 8});ImGui::Separator();ImGui::Dummy({0, 10});
    ImGui::TextWrapped("テラスのベンチでは、少女と話す事ができます。");
    ImGui::Dummy({0, 10});
    if (ImGui::BeginTable("##terrace_help", 2,
                          ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("action", ImGuiTableColumnFlags_WidthFixed,
                              190.f);
      ImGui::TableSetupColumn("description",
                              ImGuiTableColumnFlags_WidthStretch);
      const char *actions[] = {"雑談する", "ヒントを聞く", "立ち上がる"};
      const char *descriptions[] = {"物語の合間に会話する",
                                    "手掛かりについて相談する",
                                    "探索へ戻る"};
      for (int i = 0; i < 3; ++i) {
        ImGui::TableNextRow(0, 29.f);
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored({.70f, .82f, .90f, 1.f}, "%s", actions[i]);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("%s", descriptions[i]);
      }
      ImGui::EndTable();
    }
    ImGui::Dummy({0, 5});
    ImGui::Separator();
    ImGui::Dummy({0, 5});
    ImGui::TextColored({.42f, .70f, .82f, .88f},
                       "ベンチに照準を合わせる  -  F / 左クリック");
    const float w = 142.f;
    ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - w);
    if (aquariumUi::button("閉じる", {w, 36},true)) {
      tutorialPending_ = false;
      tutorialSeen_ = true;
    }
    ImGui::End();
    ImGui::PopStyleVar();
  }
  if (phase_ != Phase::Menu)
    return;
  ImGui::SetNextWindowPos({screen.x * .5f, screen.y * .66f}, ImGuiCond_Always,
                          {.5f, .5f});
  aquariumUi::PanelStyle style;
  ImGui::SetNextWindowSize({410, 0});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {28, 24});
  ImGui::Begin("##terrace_choices", nullptr,
               ImGuiWindowFlags_NoDecoration |
                   ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoSavedSettings);
  aquariumUi::drawPanelAccent();
  ImGui::TextDisabled("ヒロインと話す");ImGui::Spacing();
  const ImVec2 button{ImGui::GetContentRegionAvail().x, 42};
  if (aquariumUi::button("雑談する", button,false,{.08f,.5f})) {
    request_ = Request::Gossip;
    restDialogue_ = false;
    phase_ = Phase::WaitingDialogue;
  }
  ImGui::Dummy({0,7});
  if (aquariumUi::button("ヒントを聞く", button,false,{.08f,.5f})) {
    request_ = Request::Hint;
    restDialogue_ = false;
    phase_ = Phase::WaitingDialogue;
  }
  // Standing is always the third choice. Previously this button was the
  // unbraced body of `if (!hintAvailable)`, so it disappeared once a hint
  // became available.
  ImGui::Dummy({0,7});
  if (aquariumUi::button("立ち上がる", button,false,{.08f,.5f})) {
    phase_ = Phase::Standing;
    clock_ = 0;
  }
  ImGui::End();
  ImGui::PopStyleVar();
}
} // namespace story
