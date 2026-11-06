#pragma once

#include "Core/Math/Vector3f.h"
#include "World/Actor/Movement/Components/ClientAcceptanceThresholdsComponent.h"
#include "World/Actor/Movement/Components/MovementTraceComponent.h"

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

    MovementTraceComponent &trace() {
        return mTrace;
    }

    void reset();

    void onMovementEffect(uint64_t tick);

    // the client predicts its own exit point, which the next input reports once
    void allowDismountPosition() {
        mDismountPending = true;
    }

    /**
     * Proves that a correction reached the client: a network stack latency request goes out right after it, and
     * once the client acknowledges that request it has received the correction too. Inputs that still diverge
     * after the acknowledgement mean the client dropped it. The acknowledgement is handled on arrival while inputs
     * wait in a queue, so only inputs newer than the last one received with it can carry the correction.
     */
    bool armCorrectionCheck() {
        if (mCorrectionCheck != CorrectionCheck::None)
            return false;
        mCorrectionCheck = CorrectionCheck::AwaitingAcknowledgement;
        return true;
    }

    void acknowledgeCorrection(int64_t lastReceivedInputTick) {
        if (mCorrectionCheck != CorrectionCheck::AwaitingAcknowledgement)
            return;
        mCorrectionCheck = CorrectionCheck::Acknowledged;
        mAcknowledgedInputTick = lastReceivedInputTick;
    }

    // Returns the number of inputs that ignored an acknowledged correction so far. Only consecutive inputs count: when
    // the input queue discarded some (a burst after lag or blink), the simulation skipped them and its divergence
    // says nothing about the client, so the check starts over.
    uint32_t noteInputAfterCorrection(int64_t inputTick, bool diverging) {
        const bool consecutive = inputTick == mLastInputTick + 1;
        mLastInputTick = inputTick;
        if (mCorrectionCheck != CorrectionCheck::Acknowledged || inputTick <= mAcknowledgedInputTick)
            return 0;
        if (!diverging || !consecutive) {
            mCorrectionCheck = CorrectionCheck::None;
            mIgnoredCorrections = 0;
            return 0;
        }
        return ++mIgnoredCorrections;
    }

private:
    ClientAcceptanceThresholdsComponent mClientAcceptance;
    MovementTraceComponent mTrace;
    uint64_t mLastCorrectionTick = 0;
    enum class CorrectionCheck : uint8_t {
        None,
        AwaitingAcknowledgement,
        Acknowledged
    };

    CorrectionCheck mCorrectionCheck = CorrectionCheck::None;
    uint32_t mIgnoredCorrections = 0;
    int64_t mAcknowledgedInputTick = 0;
    int64_t mLastInputTick = 0;
    uint8_t mAdjustmentCount = 0;
    bool mDismountPending = false;
    bool mHasMovementEffect = false;
    uint64_t mLastMovementEffectTick = 0;
};
