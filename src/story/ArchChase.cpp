#include "ArchChase.h"
#include "ArchGeometry.h"
#include "../../third_party/imgui/imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace story {
namespace {
constexpr float kPi = 3.14159265f;
float Smooth(float t) {
  t = std::clamp(t, 0.f, 1.f);
  return t * t * (3.f - 2.f * t);
}
float Mix(float a, float b, float t) { return a + (b - a) * t; }
float MixAngle(float a, float b, float t) {
  return a + std::remainder(b - a, kPi * 2.f) * t;
}
} // namespace
void ArchChase::Reset() {
  phase_ = Phase::Dormant;
  request_ = Request::None;
  clock_ = 0;
  havePreviousZ_ = false;
  dialogueObserved_ = false;
  distanceWalked_ = extension_ = 0;
  failureFade_ = 0;
}
bool ArchChase::BlocksPlayer() const {
  return phase_ == Phase::WaitStagnation || phase_ == Phase::Creak ||
         phase_ == Phase::WaitCreak || phase_ == Phase::Reveal ||
         phase_ == Phase::WaitEscape || phase_ == Phase::Impact ||
         phase_ == Phase::Devour || phase_ == Phase::GameOver ||
         phase_ == Phase::WaitAftermath;
}
bool ArchChase::ConstrainsPlayer() const {
  // Only authored camera beats constrain movement; the extended corridor
  // uses normal input and never resets the player to simulate walking.
  return BlocksPlayer();
}
bool ArchChase::Complete() const { return phase_ == Phase::Complete; }
bool ArchChase::Active() const {
  return phase_ != Phase::Dormant && phase_ != Phase::Complete;
}
ArchChase::Request ArchChase::ConsumeRequest() {
  const Request r = request_;
  request_ = Request::None;
  return r;
}
void ArchChase::BeginDialogueWait(Phase phase, Request request) {
  phase_ = phase;
  request_ = request;
  dialogueObserved_ = false;
  clock_ = 0;
}
void ArchChase::ApplyCamera(AquariumSettings &s, float amount) {
  const float tremor = std::sin(clock_ * 31.f) * .006f * amount +
                       std::sin(clock_ * 17.3f) * .004f * amount;
  s.cameraYaw += tremor;
  s.cameraPitch += std::cos(clock_ * 26.f) * .004f * amount;
}
void ArchChase::Update(float dt, bool eligible, bool movingTowardEntrance,
                       bool dialogueActive, AquariumSettings &s) {
  s.horrorIntensity = 0;
  s.horrorNoise = 0;
  s.horrorVignette = 0;
  s.archFishFlee = 0;
  s.archPredatorVisibility = 0;
  s.archPredatorApproach = 0;
  s.archExtension = extension_;
  const float z = s.cameraPositionZ;
  if (phase_ == Phase::Dormant) {
    if (eligible && havePreviousZ_ && z > 43.f && z <= arch::loopTrigger &&
        std::abs(s.cameraPositionX+10.05f)<3.1f && s.cameraPositionY<0.f &&
        movingTowardEntrance) {
      anchor_ = {s.cameraPositionX, s.cameraPositionY, s.cameraPositionZ};
      anchorYaw_ = s.cameraYaw;
      anchorPitch_ = s.cameraPitch;
      clock_ = 0;
      distanceWalked_ = extension_ = 0;
      phase_ = Phase::Stagnation;
    }
    previousZ_ = z;
    havePreviousZ_ = true;
    return;
  }
  if (phase_ == Phase::Complete)
    return;
  clock_ += dt;
  if (phase_ == Phase::Stagnation) {
    // Actual movement grows the inserted span. Both exits and all geometry
    // stay in one continuous coordinate space; the camera never wraps.
    const float progress=movingTowardEntrance?std::max(0.f,previousZ_-z):0.f;
    distanceWalked_+=progress;
    extension_+=progress;
    s.archExtension=extension_;
    if (distanceWalked_ >= 30.f) {
      anchor_ = {s.cameraPositionX, s.cameraPositionY, s.cameraPositionZ};
      anchorYaw_ = s.cameraYaw;
      anchorPitch_ = s.cameraPitch;
      BeginDialogueWait(Phase::WaitStagnation, Request::StagnationDialogue);
    }
  } else if (phase_ == Phase::WaitStagnation) {
    dialogueObserved_ |= dialogueActive;
    if (dialogueObserved_ && !dialogueActive) {
      phase_ = Phase::Creak;
      clock_ = 0;
      request_ = Request::Creak;
    }
  } else if (phase_ == Phase::Creak) {
    const float rise = Smooth(clock_ / 3.6f);
    s.horrorIntensity = rise * .42f;
    s.horrorNoise = rise * .28f;
    s.horrorVignette = .20f + rise * .30f;
    s.archFishFlee = Smooth((clock_ - .8f) / 2.2f);
    ApplyCamera(s, rise);
    if (clock_ >= 2.15f)
      BeginDialogueWait(Phase::WaitCreak, Request::CreakDialogue);
  } else if (phase_ == Phase::WaitCreak) {
    s.horrorIntensity = .48f;
    s.horrorNoise = .32f;
    s.horrorVignette = .48f;
    s.archFishFlee = 1.f;
    ApplyCamera(s, .75f);
    dialogueObserved_ |= dialogueActive;
    if (dialogueObserved_ && !dialogueActive) {
      phase_ = Phase::Reveal;
      clock_ = 0;
      request_ = Request::PredatorAppear;
    }
  } else if (phase_ == Phase::Reveal) {
    const float look = Smooth(clock_ / 2.0f);
    s.cameraPositionX = anchor_.x;
    s.cameraPositionY = anchor_.y;
    s.cameraPositionZ = anchor_.z;
    s.cameraYaw = MixAngle(anchorYaw_, kPi * .5f, look);
    s.cameraPitch = Mix(anchorPitch_, -.035f, look);
    s.horrorIntensity = .55f + look * .28f;
    s.horrorNoise = .35f + look * .25f;
    s.horrorVignette = .62f + look*.08f;
    s.archFishFlee = 1.f;
    s.archPredatorVisibility = Smooth((clock_ - .55f) / 3.0f);
    ApplyCamera(s, .85f);
    if (clock_ >= 4.2f)
      BeginDialogueWait(Phase::WaitEscape, Request::EscapeDialogue);
  } else if (phase_ == Phase::WaitEscape) {
    s.cameraPositionX = anchor_.x;
    s.cameraPositionY = anchor_.y;
    s.cameraPositionZ = anchor_.z;
    s.cameraYaw = kPi * .5f;
    s.cameraPitch = -.035f;
    s.horrorIntensity = .88f;
    s.horrorNoise = .62f;
    s.horrorVignette = .70f;
    s.archFishFlee = 1.f;
    s.archPredatorVisibility = 1.f;
    ApplyCamera(s, 1.f);
    dialogueObserved_ |= dialogueActive;
    if (dialogueObserved_ && !dialogueActive) {
      phase_ = Phase::Chase;
      clock_ = 0;
      timeLimit_=arch::EscapeSeconds(s.cameraPositionZ,extension_,2.35f*1.85f);
      s.cameraYaw = kPi;
      s.cameraPitch = -.02f;
    }
  } else if (phase_ == Phase::Chase) {
    const float pressure = Smooth(clock_ / timeLimit_);
    // Accelerating pressure pulses remain visual-only: no forced look or
    // displacement, so sprinting and mouse input stay under player control.
    const float beat=std::pow(std::max(0.f,std::sin(clock_*7.f+clock_*clock_*.5f)),8.f);
    s.horrorIntensity = .42f + pressure * .34f;
    s.horrorNoise = .22f + pressure * .28f;
    // 追跡が迫るほど視界を狭め、脈打つ周辺減光で逃走の圧を出す。
    s.horrorVignette = std::min(.94f,.56f + pressure * .25f + beat*.13f);
    s.archFishFlee = 1.f;
    s.archPredatorVisibility = 1.f;
    s.archPredatorApproach = pressure;
    ApplyCamera(s, .35f + pressure * .35f + beat*.65f);
    if (z < arch::entrance-extension_) {
      phase_ = Phase::Recover;
      clock_ = 0;
    } else if (clock_ >= timeLimit_) {
      phase_ = Phase::Impact;
      clock_ = 0;
      anchor_ = {s.cameraPositionX, s.cameraPositionY, s.cameraPositionZ};
      anchorYaw_ = s.cameraYaw;
      anchorPitch_ = s.cameraPitch;
      request_ = Request::Impact;
    }
  } else if (phase_ == Phase::Impact) {
    const float hit = Smooth(clock_ / .58f);
    s.archFishFlee = 1.f;
    s.archPredatorVisibility = 1.f;
    s.archPredatorApproach = 1.f + hit * .42f;
    s.horrorIntensity = .92f;
    s.horrorNoise = .82f;
    s.horrorVignette = .78f;
    s.cameraPositionX = anchor_.x;
    s.cameraPositionY = anchor_.y;
    s.cameraPositionZ = anchor_.z;
    s.cameraYaw = anchorYaw_ + std::sin(clock_ * 79.f) * .032f * (1.f-hit*.35f);
    s.cameraPitch = anchorPitch_ + std::sin(clock_ * 63.f) * .026f * (1.f-hit*.35f);
    if (clock_ >= .58f) { phase_ = Phase::Devour; clock_ = 0; }
  } else if (phase_ == Phase::Devour) {
    const float suck = Smooth(clock_ / 1.55f);
    failureFade_ = Smooth((clock_-.32f)/1.18f);
    s.archFishFlee = 1.f;
    s.archPredatorVisibility = 1.f;
    s.archPredatorApproach = 1.42f;
    s.horrorIntensity = 1.f;
    s.horrorNoise = 1.f-failureFade_*.65f;
    s.horrorVignette = .86f;
    // The glass gives way and the viewpoint is pulled laterally into the mouth.
    s.cameraPositionX = Mix(anchor_.x, -8.15f, suck);
    s.cameraPositionY = Mix(anchor_.y, anchor_.y+.18f, suck);
    s.cameraPositionZ = anchor_.z;
    s.cameraYaw = MixAngle(anchorYaw_, kPi*.5f, suck);
    s.cameraPitch = Mix(anchorPitch_, 0.f, suck);
    ApplyCamera(s, (1.f-suck)*1.8f);
    if (clock_ >= 1.55f) { phase_ = Phase::GameOver; clock_ = 0; failureFade_=1.f; }
  } else if (phase_ == Phase::GameOver) {
    failureFade_ = 1.f;
    s.archPredatorVisibility = 1.f;
    s.archPredatorApproach = 1.42f;
  } else if (phase_ == Phase::Recover) {
    const float fade = 1.f - Smooth(clock_ / 2.2f);
    s.horrorIntensity = .5f * fade;
    s.horrorNoise = .3f * fade;
    s.horrorVignette = .42f * fade;
    s.archFishFlee = fade;
    s.archPredatorVisibility = fade;
    s.archPredatorApproach = 1.f;
    if (clock_ >= 2.2f)
      BeginDialogueWait(Phase::WaitAftermath, Request::AftermathDialogue);
  } else if (phase_ == Phase::WaitAftermath) {
    dialogueObserved_ |= dialogueActive;
    if (dialogueObserved_ && !dialogueActive)
      phase_ = Phase::Complete;
  }
  previousZ_ = s.cameraPositionZ;
  havePreviousZ_ = true;
}
void ArchChase::Draw() const {
  if (!Active())
    return;
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  if (phase_ == Phase::Chase) {
    auto *draw = ImGui::GetForegroundDrawList();
    const char *text = "走れ";
    const ImVec2 size = ImGui::CalcTextSize(text);
    draw->AddText({screen.x * .5f - size.x * .5f, screen.y * .13f},
                  IM_COL32(235, 246, 255, 225), text);
    draw->AddText({screen.x * .5f - 72.f, screen.y * .13f + 28.f},
                  IM_COL32(150, 178, 195, 190), "SHIFT  ダッシュ");
    const float remaining = std::max(0.f, timeLimit_ - clock_);
    char timer[16]{};
    std::snprintf(timer, sizeof(timer), "%04.1f", remaining);
    const ImVec2 timerSize = ImGui::CalcTextSize(timer);
    draw->AddText({screen.x * .5f - timerSize.x * .5f, screen.y * .13f + 59.f},
                  remaining > 3.f ? IM_COL32(205, 229, 238, 225)
                                  : IM_COL32(255, 91, 78, 245),
                  timer);
  }
  if(failureFade_>.001f) {
    auto* draw=ImGui::GetForegroundDrawList();
    draw->AddRectFilled({0,0},screen,IM_COL32(0,0,0,int(255.f*Smooth(failureFade_))));
    if(phase_==Phase::GameOver) {
      const char* title="GAME OVER";
      const ImVec2 titleSize=ImGui::CalcTextSize(title);
      draw->AddText({screen.x*.5f-titleSize.x*.5f,screen.y*.44f},IM_COL32(225,235,240,245),title);
      const char* prompt="クリック / F  タイトルへ";
      const ImVec2 promptSize=ImGui::CalcTextSize(prompt);
      draw->AddText({screen.x*.5f-promptSize.x*.5f,screen.y*.52f},IM_COL32(135,155,170,220),prompt);
    }
  }
}
} // namespace story
