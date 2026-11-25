#pragma once

#include "Protocol/Packet.h"

#include <vector>

class EmoteListPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::EmoteList;

    EmoteListPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "EmoteListPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint64_t mRuntimeActorId;
    std::vector<Uuid> mPieceIds;
};
