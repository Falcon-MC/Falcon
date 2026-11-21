#pragma once

#include "Network/NetworkIdentifier.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

class PacketViolationWarningPacket;
class ServerNetworkHandler;
class ServerPlayer;

// one thing the game does when asked, and how to recognise that it did
class ClientProbe {
public:
    enum class Answer : uint8_t { Unrelated, Right, Wrong };

    virtual ~ClientProbe() = default;

    // a number for the operator's log, never zero: a probe is never named, a client told which one
    // caught it answers it next time
    virtual int getCode() const = 0;

    // whether the player is in the state the probe needs
    virtual bool isReady(ServerPlayer &player) const = 0;

    virtual void send(ServerNetworkHandler &owner, ServerPlayer &player) const = 0;

    virtual Answer inspect(const PacketViolationWarningPacket &packet) const;
};

// how far one probe got with one player
struct ClientProbeProgress {
    enum class State : uint8_t { Waiting, Sent, Passed, Failed };

    // the state must hold this long first: sent on the tick a screen opens, the client may not have it yet
    static constexpr int READY_TICKS = 10;
    static constexpr int64_t ANSWER_TICKS = 100;
    // a lost answer is not a verdict: only silence every time is
    static constexpr int MAX_ATTEMPTS = 3;

    State mState = State::Waiting;
    int mReadyTicks = 0;
    int mAttempts = 0;
    int64_t mDeadline = 0;
    bool mReported = false;

    // true when the probe is to be sent now
    bool tick(bool ready, int64_t currentTick);

    void answered(bool right);

    bool isConcluded() const {
        return mState == State::Passed || mState == State::Failed;
    }
};

class ClientProbeTracker {
public:
    ClientProbeTracker();

    // the code of a probe the player just failed, zero otherwise
    int tick(ServerNetworkHandler &owner, ServerPlayer &player, int64_t currentTick);

    // true when the packet was the answer to a probe in flight
    bool onViolationWarning(ServerPlayer &player, const PacketViolationWarningPacket &packet);

    void forget(const NetworkIdentifier &id);

private:
    std::vector<std::unique_ptr<ClientProbe>> mProbes;
    std::unordered_map<NetworkIdentifier, std::vector<ClientProbeProgress>, NetworkIdentifier::Hasher> mProgress;
    // players who said something the game cannot say, asked or not
    std::unordered_map<NetworkIdentifier, int, NetworkIdentifier::Hasher> mCaught;
};
