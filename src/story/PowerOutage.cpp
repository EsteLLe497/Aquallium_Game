#include "PowerOutage.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace story {
namespace {
float Smooth(float value) {
    const float t=std::clamp(value,0.f,1.f);return t*t*(3.f-2.f*t);
}
bool InHeroHall(const AquariumSettings& s) {
    return s.cameraPositionY<0.f&&std::abs(s.cameraPositionX)<8.4f&&
           s.cameraPositionZ>1.0f&&s.cameraPositionZ<10.2f;
}
bool OnRamp(const AquariumSettings& s) {
    // 下階の床は -2.25 m、標準視点は -0.36 m。
    // 高さだけで弾くと上り口で足音が始まる前に書き文字へ到達してしまう。
    return s.cameraPositionY>-.65f&&s.cameraPositionY<4.65f&&
           s.cameraPositionX>10.0f&&s.cameraPositionX<20.8f&&
           s.cameraPositionZ>1.0f&&s.cameraPositionZ<17.2f;
}
bool BeforeRampTank(const AquariumSettings& s) {
    // The display stands in the level section just before the actual incline.
    return s.cameraPositionY>-.6f&&s.cameraPositionY<2.f&&
           s.cameraPositionX>16.8f&&s.cameraPositionX<20.8f&&s.cameraPositionZ>7.0f&&
           s.cameraPositionZ<13.8f;
}
bool AtUpperRampHandTrigger(const AquariumSettings& s) {
    // 水槽横では出さず、その先の曲がり角を抜けた長い上り坂で発火する。
    return s.cameraPositionY>.20f&&s.cameraPositionY<5.8f&&
           s.cameraPositionX>-10.3f&&s.cameraPositionX<11.2f&&
           std::abs(s.cameraPositionZ)>18.0f&&std::abs(s.cameraPositionZ)<22.7f;
}
bool AtUpperHeroTank(const AquariumSettings& s) {
    return s.cameraPositionY>4.45f&&std::abs(s.cameraPositionX)<8.8f&&
           s.cameraPositionZ>4.5f&&s.cameraPositionZ<8.0f;
}
}

void PowerOutage::Reset() {
    phase_=Phase::Dormant;requests_.fill(Request::None);requestRead_=requestCount_=0;
    clock_=restoreClock_=0;writingClock_=handClock_=fishClock_=-1;
    dialogueObserved_=writingSeen_=handsSeen_=fishSeen_=false;
    footstepsPlaying_=false;handSoundIndex_=fishImpactIndex_=0;
}

void PowerOutage::RestoreProgress(
    bool started,bool writingSeen,bool handsSeen,bool fishSeen,bool restored) {
    Reset();writingSeen_=writingSeen;handsSeen_=handsSeen;fishSeen_=fishSeen;
    phase_=restored?Phase::Complete:(started?Phase::Exploration:Phase::Dormant);
}

void PowerOutage::Queue(Request request) {
    if(request==Request::None||requestCount_>=requests_.size())return;
    requests_[(requestRead_+requestCount_)%requests_.size()]=request;
    ++requestCount_;
}

PowerOutage::Request PowerOutage::ConsumeRequest() {
    if(!requestCount_)return Request::None;
    const auto value=requests_[requestRead_];
    requests_[requestRead_]=Request::None;
    requestRead_=(requestRead_+1)%requests_.size();--requestCount_;
    return value;
}

void PowerOutage::Update(float dt,bool eligible,bool dialogueActive,
                         const AquariumSettings& camera,AquariumSettings& out) {
    out.powerOutage=0;out.flashlightOn=1;out.blackoutPredatorVisibility=0;out.blackoutPredatorApproach=0;
    out.blackoutWriting=0;
    // 10秒相当は全表示済み。出現直後だけ実時間をシェーダーへ渡す。
    out.blackoutHands=(handsSeen_&&phase_!=Phase::Complete)?10.f:-1.f;
    if(phase_==Phase::Dormant) {
        if(eligible&&InHeroHall(camera)) {
            phase_=Phase::WaitDialogue;clock_=0;dialogueObserved_=false;
            Queue(Request::BlackoutDialogue);
        }
        return;
    }
    if(phase_==Phase::Complete) {
        return;
    }

    clock_+=dt;
    if(phase_==Phase::WaitDialogue) {
        out.flashlightOn=0;
        out.powerOutage=Smooth(clock_/.35f);
        dialogueObserved_|=dialogueActive;
        if(dialogueObserved_&&!dialogueActive){phase_=Phase::Exploration;clock_=0;}
        return;
    }
    if(phase_==Phase::Restoring) {
        restoreClock_+=dt;out.powerOutage=1-Smooth((restoreClock_-.35f)/1.65f);
        if(restoreClock_>=2.f){phase_=Phase::Complete;Queue(Request::PowerRestored);}
        return;
    }

    out.powerOutage=1;
    UpdateRampEvents(dt,camera,out);
    UpdateFishEvent(dt,camera,out);
}

void PowerOutage::UpdateRampEvents(float dt,const AquariumSettings& camera,AquariumSettings& out) {
    const bool ramp=OnRamp(camera);

    // 上階からスロープへ踏み込んだ時点で、背後から追う足音を始める。
    const bool shouldPlay=ramp&&!writingSeen_;
    if(shouldPlay&&!footstepsPlaying_){footstepsPlaying_=true;Queue(Request::FootstepsStart);}
    else if(!shouldPlay&&footstepsPlaying_){footstepsPlaying_=false;Queue(Request::FootstepsStop);}

    if(!writingSeen_&&BeforeRampTank(camera)) {
        writingSeen_=true;writingClock_=0;
        Queue(Request::Bang);
    }
    if(writingClock_>=0) {
        writingClock_+=dt;
        out.blackoutWriting=Smooth(writingClock_/.10f)*(1-Smooth((writingClock_-1.45f)/.65f));
        if(writingClock_>2.2f)writingClock_=-1;
    }

    // 曲がり角後の固定地点で、左右の壁面手形とSEを一度だけ発火する。
    if(writingSeen_&&!handsSeen_&&AtUpperRampHandTrigger(camera)) {
        handsSeen_=true;handClock_=0;handSoundIndex_=0;
    }
    if(handClock_>=0) {
        handClock_+=dt;
        // 各手形の遅延は壁面シェーダー側で処理する。
        out.blackoutHands=handClock_;
        // シェーダーと同じ順序で、左右8枚それぞれの出現時にSEを要求する。
        constexpr float soundTimes[]{.04f,.15f,.27f,.38f,.50f,.61f,.73f,.84f};
        while(handSoundIndex_<8&&handClock_>=soundTimes[handSoundIndex_]) {
            Queue(Request::Hands);++handSoundIndex_;
        }
        const float pulse=std::sin(std::min(handClock_/2.35f,1.f)*3.14159265f);
        out.horrorVignette=std::max(out.horrorVignette,.34f+pulse*.38f);
        out.horrorIntensity=std::max(out.horrorIntensity,pulse*.22f);
        if(handClock_>2.35f){handClock_=-1;out.blackoutHands=10.f;}
    }

}

void PowerOutage::UpdateFishEvent(float dt,const AquariumSettings& camera,AquariumSettings& out) {
    if(!fishSeen_&&AtUpperHeroTank(camera)) {
        fishSeen_=true;fishClock_=0;fishImpactIndex_=0;
    }
    if(fishClock_<0)return;
    fishClock_+=dt;
    constexpr float impacts[]={.72f,1.78f,2.92f};
    if(fishImpactIndex_<3&&fishClock_>=impacts[fishImpactIndex_]) {
        ++fishImpactIndex_;Queue(Request::FishImpact);
    }
    const float pulse0=std::exp(-std::pow((fishClock_-.72f)/.20f,2.f));
    const float pulse1=std::exp(-std::pow((fishClock_-1.78f)/.18f,2.f));
    const float pulse2=std::exp(-std::pow((fishClock_-2.92f)/.16f,2.f));
    const float pulse=std::max({pulse0,pulse1,pulse2});
    out.blackoutPredatorVisibility=Smooth(fishClock_/.38f)*
        (1-Smooth((fishClock_-3.45f)/1.15f));
    out.blackoutPredatorApproach=.18f+.82f*pulse;
    out.horrorIntensity=std::max(out.horrorIntensity,pulse*.72f);
    out.horrorNoise=std::max(out.horrorNoise,pulse*.32f);
    out.horrorVignette=std::max(out.horrorVignette,.2f+pulse*.35f);
    if(fishClock_>4.7f)fishClock_=-1;
}

void PowerOutage::ConfirmRestore() {
    if(phase_!=Phase::Exploration)return;
    phase_=Phase::Restoring;restoreClock_=0;
}
}
