#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/GameRuleData.h"

#include <vector>

class GameRulesChangedPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::GameRulesChanged;

    GameRulesChangedPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "GameRulesChangedPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<ChangedGameRuleData> mGameRules;
};
