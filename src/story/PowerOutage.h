#pragma once

#include "../rendering/AquariumRenderer.h"

namespace story {

// 水中アーチ脱出後の停電探索を一つの状態として管理する。
// 座標判定は演出の開始だけに使い、移動やテラス利用は制限しない。
class PowerOutage {
public:
    enum class Request {None,BlackoutDialogue,FootstepsStart,FootstepsStop,Bang,FishImpact,PowerRestored};

    void Reset();
    void RestoreProgress(bool started,bool writingSeen,bool fishSeen,bool restored);
    void Update(float dt,bool eligible,bool dialogueActive,const AquariumSettings& camera,
                AquariumSettings& presentation);
    void ConfirmRestore();
    Request ConsumeRequest();

    bool BlocksPlayer() const {return false;}
    bool WantsCursor() const {return false;}
    bool Started() const {return phase_!=Phase::Dormant;}
    bool BlackedOut() const {return phase_==Phase::WaitDialogue||phase_==Phase::Exploration||phase_==Phase::Restoring;}
    bool Restored() const {return phase_==Phase::Complete;}
    bool WritingSeen() const {return writingSeen_;}
    bool FishSeen() const {return fishSeen_;}

private:
    enum class Phase {Dormant,WaitDialogue,Exploration,Restoring,Complete};
    void Queue(Request request);
    void UpdateRampEvents(float dt,const AquariumSettings& camera,AquariumSettings& presentation);
    void UpdateFishEvent(float dt,const AquariumSettings& camera,AquariumSettings& presentation);

    Phase phase_=Phase::Dormant;
    Request request_=Request::None;
    float clock_=0,writingClock_=-1,fishClock_=-1,restoreClock_=0;
    bool dialogueObserved_=false,writingSeen_=false,fishSeen_=false;
    bool footstepsPlaying_=false;
    int fishImpactIndex_=0;
};
}
