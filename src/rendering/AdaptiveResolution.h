#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace rendering
{
// Discrete resolution scaling avoids per-frame resource churn. It reacts to a
// sustained GPU/CPU frame-time deficit and requires generous headroom before
// increasing quality, so the renderer does not oscillate around its budget.
class AdaptiveResolution
{
public:
    void Update(float deltaTime, bool enabled, float targetFramesPerSecond)
    {
        if (!enabled)
        {
            tier_ = 0;
            evaluationTimer_ = 0.0f;
            smoothedFrameTime_ = 0.0f;
            return;
        }
        if (deltaTime <= 0.0f || deltaTime > 0.050f)
        {
            return;
        }

        if (smoothedFrameTime_ <= 0.0f)
        {
            smoothedFrameTime_ = deltaTime;
        }
        else
        {
            const float blend = 1.0f - std::exp(-deltaTime * 3.5f);
            smoothedFrameTime_ +=
                (deltaTime - smoothedFrameTime_) * blend;
        }
        evaluationTimer_ += deltaTime;
        if (evaluationTimer_ < 0.80f)
        {
            return;
        }
        evaluationTimer_ = 0.0f;

        const float targetFrameTime =
            1.0f / (targetFramesPerSecond > 30.0f
                ? targetFramesPerSecond : 100.0f);
        if (smoothedFrameTime_ > targetFrameTime * 1.05f &&
            tier_ + 1 < kScales.size())
        {
            ++tier_;
        }
        // A 9% guard band is enough for the discrete next tier. The former
        // 24% requirement could strand the renderer at 82% after an expensive
        // loading frame even while the steady-state view was well over target.
        else if (smoothedFrameTime_ < targetFrameTime * 0.91f &&
                 tier_ > 0)
        {
            --tier_;
        }
    }

    [[nodiscard]] float Scale() const noexcept
    {
        return kScales[tier_];
    }

    [[nodiscard]] float SmoothedFrameMilliseconds() const noexcept
    {
        return smoothedFrameTime_ * 1000.0f;
    }

private:
    static constexpr std::array<float, 5> kScales{
        1.00f, 0.90f, 0.82f, 0.76f, 0.70f};
    std::size_t tier_ = 0;
    float evaluationTimer_ = 0.0f;
    float smoothedFrameTime_ = 0.0f;
};
}
