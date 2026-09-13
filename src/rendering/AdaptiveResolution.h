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
        // Resolution may rise only with enough headroom to pay for the next
        // tier's additional pixels. A small symmetric guard caused the hero
        // overlook to oscillate all the way between 55% and 100%, producing
        // unstable frame pacing. Cheap rooms still recover quality, but a
        // genuinely GPU-bound view now settles at one tier.
        else if (smoothedFrameTime_ < targetFrameTime * 0.72f &&
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
    // The former 70% floor could not recover the 100 FPS budget when the
    // two-storey hero pane filled the frame.  Two additional tiers are used
    // only after a sustained deficit; ordinary rooms remain at their higher
    // tiers, while the worst-case overlook can trade pixels for frame time.
    static constexpr std::array<float, 7> kScales{
        1.00f, 0.90f, 0.82f, 0.76f, 0.70f, 0.62f, 0.55f};
    std::size_t tier_ = 0;
    float evaluationTimer_ = 0.0f;
    float smoothedFrameTime_ = 0.0f;
};
}
