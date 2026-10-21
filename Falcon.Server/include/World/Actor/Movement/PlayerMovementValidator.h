#pragma once

#include "Core/Math/Vector3f.h"

#include <cstdint>

/** Position reconciliation policy, independent of travel physics and client velocity. */
class PlayerMovementValidator {
public:
    bool reconcile(Vector3f &simulated, const Vector3f &reported, uint64_t tick, float threshold, bool strict);

    void reset();

    void onMovementEffect(uint64_t tick);

private:
    uint64_t mLastCorrectionTick = 0;
    uint8_t mAdjustmentCount = 0;
    bool mHasMovementEffect = false;
    uint64_t mLastMovementEffectTick = 0;
};
