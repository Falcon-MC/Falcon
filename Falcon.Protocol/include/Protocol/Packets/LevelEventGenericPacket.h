#pragma once

#include "Core/NBT/Tag.h"
#include "Protocol/Packet.h"

class LevelEventGenericPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::LevelEventGeneric;

    enum Event : int32_t {
        ParticleBlockExplode = 2026
    };

    LevelEventGenericPacket();

    MinecraftPacketIds getId() const override {
        return ID;
    }

    const char *getName() const override {
        return "LevelEventGenericPacket";
    }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int32_t mEventId;
    Tag mData;
};
