#include "World/Actor/Movement/ServerPlayerMovementComponent.h"

#include "Protocol/Packets/PlayerAuthInputPacket.h"

#include <algorithm>
#include <cmath>
#include <limits>

bool ServerPlayerMovementComponent::enqueue(const PlayerAuthInputPacket &packet) {
    if (packet.mTick < 0 || packet.mTick <= mLastReceivedTick || mInputs.size() >= MAX_PENDING_INPUTS)
        return false;

    mInputs.push_back(std::make_shared<PlayerAuthInputPacket>(packet));
    mLastReceivedTick = packet.mTick;
    return true;
}

void ServerPlayerMovementComponent::beginTick(int64_t serverTick, int maxCredits, int batchSize) {
    if (serverTick < 0)
        return;

    mBatchSize = batchSize < 0 ? MAX_PENDING_INPUTS : (size_t) batchSize;
    const uint64_t limit = maxCredits < 0 ? std::numeric_limits<uint64_t>::max() : (uint64_t) maxCredits;
    const uint64_t elapsed = mLastServerTick < 0 ? 1
                             : serverTick > mLastServerTick ? (uint64_t) (serverTick - mLastServerTick) : 0;
    mCredits = std::min(mCredits, limit);
    mCredits += std::min(elapsed, limit - mCredits);
    mLastServerTick = std::max(mLastServerTick, serverTick);
}

std::shared_ptr<const PlayerAuthInputPacket> ServerPlayerMovementComponent::takeNext() {
    // BDS 1.26.10 tick policy (0x140B45360): discard old inputs beyond credits + 1 or batch size.
    while (!mInputs.empty()) {
        if (mCredits == 0 && mInputs.size() <= 1)
            return nullptr;
        if (mCredits >= mInputs.size() - 1 && mBatchSize >= mInputs.size())
            break;
        mInputs.pop_front();
    }

    if (mInputs.empty() || mCredits == 0)
        return nullptr;

    auto packet = std::move(mInputs.front());
    mInputs.pop_front();
    --mCredits;
    mLastProcessedTick = packet->mTick;
    return packet;
}

void ServerPlayerMovementComponent::deferFlightRequest(bool flying) {
    if (!mFlightRequests.empty() && mFlightRequests.back().afterTick == mLastReceivedTick) {
        mFlightRequests.back().flying = flying;
    } else if (mFlightRequests.size() < MAX_PENDING_INPUTS) {
        mFlightRequests.push_back({mLastReceivedTick, flying});
    }
}

bool ServerPlayerMovementComponent::takeFlightRequest(int64_t inputTick, bool inclusive, bool &flying) {
    if (mFlightRequests.empty() || mFlightRequests.front().afterTick > inputTick
        || (!inclusive && mFlightRequests.front().afterTick == inputTick))
        return false;
    flying = mFlightRequests.front().flying;
    mFlightRequests.pop_front();
    return true;
}

void ServerPlayerMovementComponent::clearPending() {
    // A teleport invalidates pending coordinates, but must not reopen the accepted tick sequence.
    mInputs.clear();
    mFlightRequests.clear();
}

size_t ServerPlayerMovementComponent::catchupSteps(double previousTickMilliseconds) {
    // BDS catchup loop (0x1432502B0): 2 * min(elapsed 50 ms steps, 20) + 1.
    if (!std::isfinite(previousTickMilliseconds) || previousTickMilliseconds <= 0.0)
        return 1;
    const size_t elapsed = (size_t) std::min(previousTickMilliseconds / 50.0, 20.0);
    return 2 * elapsed + 1;
}
