#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cstdint>

class UpdatePlayerGameTypePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::UpdatePlayerGameType;

    UpdatePlayerGameTypePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "UpdatePlayerGameTypePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    GameType mGameType = GameType::Survival;
    int64_t mActorId = 0;
    int64_t mTick = 0;
};
