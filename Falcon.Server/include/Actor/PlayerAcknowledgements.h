#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

class NetworkIdentifier;
class ServerNetworkHandler;

class PlayerAcknowledgements {
public:
    void add(std::function<void()> action);

    void flush(ServerNetworkHandler &owner, const NetworkIdentifier &id);

    bool acknowledge(uint64_t timestamp);

    bool hasTimedOut(int64_t currentTick) const;

    void clear();

private:
    struct Batch {
        uint64_t mTimestamp = 0;
        int64_t mSentTick = 0;
        std::vector<std::function<void()>> mActions;
    };

    static bool _matches(uint64_t sent, uint64_t received);

    std::vector<std::function<void()>> mCurrent;
    std::deque<Batch> mPending;
    uint64_t mNextTimestamp = 1;
};
