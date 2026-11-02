#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

class PlayerAuthInputPacket;

/** Main-thread input timeline. Network arrival never advances movement simulation itself. */
class ServerPlayerMovementComponent {
public:
    // BDS 1.26.10 ServerPlayerMovementComponent input admission (0x14127B1A0).
    static constexpr size_t MAX_PENDING_INPUTS = 1000;

    bool enqueue(const PlayerAuthInputPacket &packet);

    void beginTick(int64_t serverTick, int maxCredits, int batchSize);

    std::shared_ptr<const PlayerAuthInputPacket> takeNext();

    void clearPending();
    // Inputs dropped unprocessed since the last call; their predicted block changes must be resent to the client.
    std::vector<std::shared_ptr<const PlayerAuthInputPacket>> takeDropped() {
        std::vector<std::shared_ptr<const PlayerAuthInputPacket>> dropped;
        dropped.swap(mDropped);
        return dropped;
    }

    bool hasPendingFlightRequest() const {
        return !mFlightRequests.empty();
    }

    void deferFlightRequest(bool flying);

    bool takeFlightRequest(int64_t inputTick, bool inclusive, bool &flying);

    size_t pendingCount() const {
        return mInputs.size();
    }

    uint64_t credits() const {
        return mCredits;
    }

    int64_t lastProcessedTick() const {
        return mLastProcessedTick;
    }

    int64_t lastReceivedTick() const {
        return mLastReceivedTick;
    }

    // Counts consecutive inputs whose tick does not advance past the last received one; returns the current streak.
    uint32_t noteInputTick(int64_t tick) {
        mStaleTicks = mLastReceivedTick >= 0 && tick <= mLastReceivedTick ? mStaleTicks + 1 : 0;
        return mStaleTicks;
    }

    static size_t catchupSteps(double previousTickMilliseconds);

private:
    struct FlightRequest {
        int64_t afterTick;
        bool flying;
    };

    std::deque<std::shared_ptr<const PlayerAuthInputPacket>> mInputs;
    std::deque<FlightRequest> mFlightRequests;
    std::vector<std::shared_ptr<const PlayerAuthInputPacket>> mDropped;
    int64_t mLastReceivedTick = -1;
    uint32_t mStaleTicks = 0;
    int64_t mLastProcessedTick = -1;
    int64_t mLastServerTick = -1;
    uint64_t mCredits = 0;
    size_t mBatchSize = MAX_PENDING_INPUTS;
};
