#pragma once

#include "Core/Math/Vector3f.h"
#include "World/Actor/Movement/Components/ClientAcceptanceThresholdsComponent.h"

#include <cstdint>

struct MovementCorrectionDetails {
    const char *reason = nullptr;
    Vector3f error;
    float distanceSquared = 0.0f;
    float threshold = 0.0f;
    float fineThreshold = 0.0f;
    uint8_t adjustments = 0;
    uint64_t previousCorrectionTick = 0;
};

/** Position reconciliation policy, independent of travel physics and client velocity. */
class PlayerMovementValidator {
public:
    bool reconcile(Vector3f &simulated, const Vector3f &reported, uint64_t tick, float threshold, bool strict,
                   MovementCorrectionDetails *details = nullptr);

    ClientAcceptanceThresholdsComponent &clientAcceptance() {
        return mClientAcceptance;
    }

    void reset();

    void onMovementEffect(uint64_t tick);

private:
    ClientAcceptanceThresholdsComponent mClientAcceptance;
    uint64_t mLastCorrectionTick = 0;
    uint8_t mAdjustmentCount = 0;
    bool mHasMovementEffect = false;
    uint64_t mLastMovementEffectTick = 0;
};
