#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

enum class VideoStreamConnectAction : unsigned char {
    Open = 0,
    Close = 1
};

class VideoStreamConnectPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::VideoStreamConnect;

    VideoStreamConnectPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "VideoStreamConnectPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mAddress;
    float mScreenshotFrequency = 0.0f;
    VideoStreamConnectAction mAction = VideoStreamConnectAction::Open;
    int32_t mWidth = 0;
    int32_t mHeight = 0;
};
