#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/MusicPlaybackTypes.h"

#include <cstdint>
#include <string>

class ClientboundOnlineMusicPlaybackResponsePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ClientboundOnlineMusicPlaybackResponse;

    ClientboundOnlineMusicPlaybackResponsePacket();

    MinecraftPacketIds getId() const override {
        return ID;
    }

    const char *getName() const override {
        return "ClientboundOnlineMusicPlaybackResponsePacket";
    }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint32_t mRequestId = 0;
    std::string mState = MusicPlaybackState::ONLINE;
    std::string mSharedMetadata;
    std::string mPlaybackContent;
};
