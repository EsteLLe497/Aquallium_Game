#pragma once

#include <cstdint>

namespace framework
{
struct FrameContext
{
    float deltaTime = 0.0f;
    double totalTime = 0.0;
    std::uint64_t frameIndex = 0;
};
}
