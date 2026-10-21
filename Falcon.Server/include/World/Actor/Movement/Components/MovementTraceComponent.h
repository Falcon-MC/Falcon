#pragma once

#include "Core/Math/Vector3f.h"

#include <array>
#include <cstddef>
#include <cstdint>

struct MovementTraceFrame {
    uint64_t tick = 0;
    uint64_t flagsLow = 0;
    uint64_t flagsHigh = 0;
    Vector3f start;
    Vector3f reported;
    Vector3f predicted;
    Vector3f previousVelocity;
    Vector3f physicalVelocity;
    Vector3f accepted;
    Vector3f clientDelta;
    Vector3f rotation;
    float motionX = 0.0f;
    float motionY = 0.0f;
    float acceptanceBudget = 0.0f;
    uint32_t state = 0;
};

/** Bounded replay history, formatted only when a correction is emitted. */
class MovementTraceComponent {
public:
    void record(const MovementTraceFrame &frame) {
        mFrames[mNext] = frame;
        mNext = (mNext + 1) % mFrames.size();
        if (mCount < mFrames.size())
            ++mCount;
    }

    size_t size() const {
        return mCount;
    }

    const MovementTraceFrame &at(size_t index) const {
        return mFrames[(mNext + mFrames.size() - mCount + index) % mFrames.size()];
    }

    void clear() {
        mCount = 0;
        mNext = 0;
    }

private:
    std::array<MovementTraceFrame, 32> mFrames{};
    size_t mCount = 0;
    size_t mNext = 0;
};
