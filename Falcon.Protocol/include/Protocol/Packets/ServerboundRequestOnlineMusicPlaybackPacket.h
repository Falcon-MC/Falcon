#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class ServerboundRequestOnlineMusicPlaybackPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ServerboundRequestOnlineMusicPlayback;

    ServerboundRequestOnlineMusicPlaybackPacket();

    MinecraftPacketIds getId() const override {
        return ID;
    }

    const char *getName() const override {
        return "ServerboundRequestOnlineMusicPlaybackPacket";
    }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint32_t mRequestId = 0;
    std::string mSoundEventName;
};
