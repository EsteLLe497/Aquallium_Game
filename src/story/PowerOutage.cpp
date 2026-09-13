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
    return s.cameraPositionY>-.35f&&s.cameraPositionY<4.65f&&
           s.cameraPositionX>10.0f&&s.cameraPositionX<20.8f&&
           s.cameraPositionZ>1.0f&&s.cameraPositionZ<17.2f;
}
bool BeforeRampTank(const AquariumSettings& s) {
    // The display stands in the level section just before the actual incline.
    return s.cameraPositionY>-.6f&&s.cameraPositionY<2.f&&
           s.cameraPositionX>16.8f&&s.cameraPositionX<20.8f&&s.cameraPositionZ>7.0f&&
           s.cameraPositionZ<13.8f;
}
bool AtUpperHeroTank(const AquariumSettings& s) {
    return s.cameraPositionY>4.45f&&std::abs(s.cameraPositionX)<8.8f&&
           s.cameraPositionZ>4.5f&&s.cameraPositionZ<8.0f;
}
}

void PowerOutage::Reset() {
    phase_=Phase::Dormant;request_=Request::None;clock_=restoreClock_=0;
    writingClock_=fishClock_=-1;dialogueObserved_=writingSeen_=fishSeen_=false;
    footstepsPlaying_=false;fishImpactIndex_=0;
}

void PowerOutage::RestoreProgress(bool started,bool writingSeen,bool fishSeen,bool restored) {
    Reset();writingSeen_=writingSeen;fishSeen_=fishSeen;
    phase_=restored?Phase::Complete:(started?Phase::Exploration:Phase::Dormant);
}

void PowerOutage::Queue(Request request) {
    // A newly entered trigger must not lose its glass hit to a footstep start.
    if(request_==Request::None || (request==Request::Bang&&request_==Request::FootstepsStart))
        request_=request;
}

PowerOutage::Request PowerOutage::ConsumeRequest() {
    const auto value=request_;request_=Request::None;return value;
}

void PowerOutage::Update(float dt,bool eligible,bool dialogueActive,
                         const AquariumSettings& camera,AquariumSettings& out) {
    out.powerOutage=0;out.flashlightOn=1;out.blackoutPredatorVisibility=0;out.blackoutPredatorApproach=0;
    out.blackoutWriting=0;
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
    if(!writingSeen_&&BeforeRampTank(camera)) {
        writingSeen_=true;writingClock_=0;
        Queue(Request::Bang);
    }
    if(writingClock_>=0) {
        writingClock_+=dt;
        out.blackoutWriting=Smooth(writingClock_/.10f)*(1-Smooth((writingClock_-1.45f)/.65f));
        if(writingClock_>2.2f)writingClock_=-1;
    }
    const bool shouldPlay=ramp&&writingSeen_&&writingClock_<0;
    if(shouldPlay&&!footstepsPlaying_){footstepsPlaying_=true;Queue(Request::FootstepsStart);}
    else if(!shouldPlay&&footstepsPlaying_){footstepsPlaying_=false;Queue(Request::FootstepsStop);}
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
