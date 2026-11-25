#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/ScoreInfo.h"

#include <vector>

class SetScorePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::SetScore;

    SetScorePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "SetScorePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    enum class Action : uint8_t {
        Change = 0,
        Remove = 1
    };

    Action mAction = Action::Change;
    std::vector<ScoreInfoEntry> mInfos;
};
