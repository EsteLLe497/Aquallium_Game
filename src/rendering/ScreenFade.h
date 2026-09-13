#pragma once
#include <algorithm>
#include <utility>

namespace rendering {
// Reversible full-screen transition. The scene mutation is invoked exactly
// once at full black, then the newly selected state fades in.
class ScreenFade {
public:
    enum class Phase {Idle,Out,In};

    // Scenes begin hidden so the first presented frame also fades in.
    ScreenFade()=default;
    bool BeginOut() noexcept {
        if(phase_==Phase::Out)return false;
        phase_=Phase::Out;
        return true;
    }
    template<class ApplyAtBlack>
    bool Update(float deltaTime,ApplyAtBlack&& applyAtBlack) {
        const bool wasActive=Active();
        const float dt=std::clamp(deltaTime,0.f,.1f);
        if(phase_==Phase::Out) {
            alpha_=std::min(1.f,alpha_+dt/outDuration_);
            if(alpha_>=1.f) {
                std::forward<ApplyAtBlack>(applyAtBlack)();
                phase_=Phase::In;
            }
        } else if(phase_==Phase::In) {
            alpha_=std::max(0.f,alpha_-dt/inDuration_);
            if(alpha_<=0.f)phase_=Phase::Idle;
        }
        return wasActive;
    }
    [[nodiscard]] bool Active() const noexcept {return phase_!=Phase::Idle;}
    [[nodiscard]] float Alpha() const noexcept {return alpha_;}
    [[nodiscard]] Phase CurrentPhase() const noexcept {return phase_;}

private:
    Phase phase_=Phase::In;
    float alpha_=1.f;
    float outDuration_=.42f,inDuration_=.62f;
};
}
