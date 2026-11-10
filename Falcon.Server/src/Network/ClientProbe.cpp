#include "Network/ClientProbe.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/ContainerClosePacket.h"
#include "Protocol/Packets/PacketViolationWarningPacket.h"
#include "World/Actor/Player/ServerPlayer.h"

namespace {
    constexpr int FORGED_VIOLATION_CODE = 2;

    const char *const CLOSE_VIOLATION =
            "Server sent a ContainerClosePacket response packet when it should have sent the server initiated packet";

    // sub_146BD7130: a close marked as a response, while a container screen is open that the client did
    // not ask to close, is reported back as a violation naming the warning packet itself. The screen
    // stays open
    class ContainerCloseProbe final : public ClientProbe {
    public:
        int getCode() const override {
            return 1;
        }

        bool isReady(ServerPlayer &player) const override {
            return player.isSpawned() && !player.isDead() && player.getInventoryManager().isContainerOpen();
        }

        void send(ServerNetworkHandler &owner, ServerPlayer &player) const override {
            const InventoryManager &manager = player.getInventoryManager();

            ContainerClosePacket close;
            close.mWindowId = (int8_t) manager.getCurrentWindowId();
            close.mType = manager.getCurrentWindowType();
            close.mServerInitiated = false;
            owner.sendPacketTo(player.getNetworkIdentifier(), close);
        }

        Answer inspect(const PacketViolationWarningPacket &packet) const override {
            if (packet.mContext.find("ContainerClosePacket") == std::string::npos)
                return Answer::Unrelated;

            const bool right = packet.mType == PacketViolationType::Unknown
                               && packet.mSeverity == PacketViolationSeverity::Warning
                               && packet.mPacketCauseId == (int32_t) MinecraftPacketIds::PacketViolationWarning
                               && packet.mContext == CLOSE_VIOLATION;
            return right ? Answer::Right : Answer::Wrong;
        }
    };
}

ClientProbe::Answer ClientProbe::inspect(const PacketViolationWarningPacket &packet) const {
    (void) packet;
    return Answer::Unrelated;
}

bool ClientProbeProgress::tick(bool ready, int64_t currentTick) {
    if (isConcluded())
        return false;

    if (mState == State::Sent) {
        // the screen closed while the question was on its way: the game had nothing left to answer
        // about, so this attempt says nothing of the client
        if (!ready) {
            mState = State::Waiting;
            mAttempts--;
            mReadyTicks = 0;
            return false;
        }
        if (currentTick < mDeadline)
            return false;

        mState = mAttempts >= MAX_ATTEMPTS ? State::Failed : State::Waiting;
        mReadyTicks = 0;
        return false;
    }

    if (!ready) {
        mReadyTicks = 0;
        return false;
    }
    if (++mReadyTicks < READY_TICKS)
        return false;

    mState = State::Sent;
    mAttempts++;
    mDeadline = currentTick + ANSWER_TICKS;
    return true;
}

void ClientProbeProgress::answered(bool right) {
    if (mState == State::Sent)
        mState = right ? State::Passed : State::Failed;
}

ClientProbeTracker::ClientProbeTracker() {
    mProbes.emplace_back(new ContainerCloseProbe());
}

int ClientProbeTracker::tick(ServerNetworkHandler &owner, ServerPlayer &player, int64_t currentTick) {
    const auto caught = mCaught.find(player.getNetworkIdentifier());
    if (caught != mCaught.end()) {
        const int code = caught->second;
        mCaught.erase(caught);
        return code;
    }

    std::vector<ClientProbeProgress> &progress = mProgress[player.getNetworkIdentifier()];
    progress.resize(mProbes.size());

    for (size_t index = 0; index < mProbes.size(); ++index) {
        ClientProbeProgress &state = progress[index];
        const ClientProbe &probe = *mProbes[index];
        if (state.tick(probe.isReady(player), currentTick))
            probe.send(owner, player);

        // a wrong answer fails between two ticks: reported from here either way
        if (state.mState == ClientProbeProgress::State::Failed && !state.mReported) {
            state.mReported = true;
            return probe.getCode();
        }
    }
    return 0;
}

bool ClientProbeTracker::onViolationWarning(ServerPlayer &player, const PacketViolationWarningPacket &packet) {
    const auto found = mProgress.find(player.getNetworkIdentifier());
    for (size_t index = 0; found != mProgress.end() && index < mProbes.size() && index < found->second.size();
         ++index) {
        ClientProbeProgress &state = found->second[index];
        if (state.mState != ClientProbeProgress::State::Sent)
            continue;

        const ClientProbe::Answer answer = mProbes[index]->inspect(packet);
        if (answer == ClientProbe::Answer::Unrelated)
            continue;

        state.answered(answer == ClientProbe::Answer::Right);
        return true;
    }

    // the game writes one violation that names the warning packet itself: any other is made up
    if (packet.mPacketCauseId == (int32_t) MinecraftPacketIds::PacketViolationWarning
        && packet.mContext != CLOSE_VIOLATION) {
        mCaught[player.getNetworkIdentifier()] = FORGED_VIOLATION_CODE;
        return true;
    }
    return false;
}

void ClientProbeTracker::forget(const NetworkIdentifier &id) {
    mProgress.erase(id);
    mCaught.erase(id);
}
