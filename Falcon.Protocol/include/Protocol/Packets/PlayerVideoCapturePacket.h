#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

enum class PlayerVideoCaptureAction : unsigned char {
    StopVideoCapture = 0,
    StartVideoCapture = 1,
    Unknown = 2
};

class PlayerVideoCapturePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PlayerVideoCapture;

    PlayerVideoCapturePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PlayerVideoCapturePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    PlayerVideoCaptureAction mAction = PlayerVideoCaptureAction::StopVideoCapture;
    int32_t mFrameRate = 0;
    std::string mFilePrefix;
};
