#include "Actor/PlayerAcknowledgements.h"

#include "Network/Handler/NetworkHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/NetworkStackLatencyPacket.h"

#include <utility>

namespace {
    const uint64_t RESPONSE_SCALE = 1000;
    const int64_t RESPONSE_TIMEOUT_TICKS = 60 * 20;
}

void PlayerAcknowledgements::add(std::function<void()> action) {
    mCurrent.push_back(std::move(action));
}

void PlayerAcknowledgements::flush(ServerNetworkHandler &owner, const NetworkIdentifier &id) {
    if (mCurrent.empty())
        return;

    Batch batch;
    batch.mTimestamp = mNextTimestamp++;
    batch.mSentTick = owner.getCurrentTick();
    batch.mActions = std::move(mCurrent);
    mCurrent.clear();

    NetworkStackLatencyPacket packet;
    packet.mTimestamp = batch.mTimestamp;
    packet.mFromServer = true;
    owner.getNetworkHandler().send(id, packet, owner.getCodecContext());
    mPending.push_back(std::move(batch));
}

bool PlayerAcknowledgements::acknowledge(uint64_t timestamp) {
    size_t index = 0;
    while (index < mPending.size() && !_matches(mPending[index].mTimestamp, timestamp))
        index++;
    if (index == mPending.size())
        return false;

    for (size_t i = 0; i <= index; i++) {
        std::vector<std::function<void()>> actions = std::move(mPending.front().mActions);
        mPending.pop_front();
        for (const std::function<void()> &action: actions)
            action();
    }
    return true;
}

void PlayerAcknowledgements::clear() {
    mCurrent.clear();
    mPending.clear();
}

bool PlayerAcknowledgements::_matches(uint64_t sent, uint64_t received) {
    return received == sent || received == sent * RESPONSE_SCALE
           || received == sent * RESPONSE_SCALE * RESPONSE_SCALE;
}

bool PlayerAcknowledgements::hasTimedOut(int64_t currentTick) const {
    return !mPending.empty() && currentTick - mPending.front().mSentTick > RESPONSE_TIMEOUT_TICKS;
}
