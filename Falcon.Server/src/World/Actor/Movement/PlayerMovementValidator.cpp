#include "World/Actor/Movement/PlayerMovementValidator.h"

#include <algorithm>
#include <cmath>

bool PlayerMovementValidator::reconcile(Vector3f &simulated, const Vector3f &reported, uint64_t tick,
                                        float threshold, bool strict) {
    // BDS 1.26.10: comparator 0x140B4A560, defaults 0x1437F5420, property scaling 0x140158FA0.
    const uint8_t adjustments = mAdjustmentCount;
    mAdjustmentCount = 0;
    if (tick <= mLastCorrectionTick || tick - mLastCorrectionTick <= 5)
        return false;

    const float dx = reported.x - simulated.x;
    const float dy = reported.y - simulated.y;
    const float dz = reported.z - simulated.z;
    const float distanceSquared = (dy * dy + dx * dx) + dz * dz;
    threshold = std::max(0.0f, threshold);
    if ((adjustments >= 10 && distanceSquared > 0.0000001f) || distanceSquared > threshold) {
        mLastCorrectionTick = tick;
        return true;
    }

    // BDS 0x14387A590/0x140B4A910: movement effects suspend client position adoption for 20 ticks.
    if (mHasMovementEffect && tick >= mLastMovementEffectTick && tick - mLastMovementEffectTick <= 20)
        return false;

    if (distanceSquared < (threshold * threshold) * 0.0001f) {
        simulated = reported;
        return false;
    }
    if (strict) {
        mLastCorrectionTick = tick;
        return true;
    }

    const float step = threshold * 0.01f;
    if (step > 0.0f && distanceSquared > 0.0000001f) {
        const float distance = std::sqrt(distanceSquared);
        const float scale = std::min(distance, step) / distance;
        simulated.x += dx * scale;
        simulated.y += dy * scale;
        simulated.z += dz * scale;
        mAdjustmentCount = adjustments + 1;
    }
    return false;
}

void PlayerMovementValidator::reset() {
    mLastCorrectionTick = 0;
    mAdjustmentCount = 0;
    mHasMovementEffect = false;
    mLastMovementEffectTick = 0;
}

void PlayerMovementValidator::onMovementEffect(uint64_t tick) {
    mHasMovementEffect = true;
    mLastMovementEffectTick = tick;
}
