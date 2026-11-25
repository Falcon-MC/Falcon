#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/ScoreboardIdentityEntry.h"

#include <vector>

class SetScoreboardIdentityPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SetScoreboardIdentity;

    enum class Action {
        Add,
        Remove
    };

    SetScoreboardIdentityPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SetScoreboardIdentityPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Action mAction = Action::Add;
    std::vector<ScoreboardIdentityEntry> mEntries;
};
