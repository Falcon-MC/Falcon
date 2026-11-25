#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class ClientboundMatchmakingStatePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ClientboundMatchmakingState;

    enum class State : uint8_t {
        Idle = 0,
        Matchmaking = 1,
        MatchFound = 2,
        Canceled = 3,
        PlayerLeftParty = 4,
        PlayerLeftServer = 5,
        ServerShutdown = 6,
        TimedOut = 7,
        RequeueAsParty = 8
    };

    ClientboundMatchmakingStatePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ClientboundMatchmakingStatePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    State mState = State::Idle;
    std::string mDestinationName;
    bool mHasOptions = false;
    bool mHasTriggeringPlayerName = false;
    std::string mTriggeringPlayerName;
    bool mHasTriggeredByLocalPlayer = false;
    bool mTriggeredByLocalPlayer = false;
};
