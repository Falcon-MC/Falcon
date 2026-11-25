#pragma once

#include "Protocol/Packet.h"

enum class SessionDisconnectReason : int32_t {
    SessionRuntimeError = 151,
    SessionInvalidResponse = 152,
    SessionNetworkError = 153,
    SessionServiceError = 154,
    SessionServiceUnavailable = 155,
    SessionThrottled = 156,
    SessionRequestRejected = 157,
    SessionUnknownError = 158
};

class DisconnectPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::Disconnect;

    DisconnectPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "DisconnectPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int32_t mReason;
    bool mMessageSkipped;
    std::string mKickMessage;
    std::string mFilteredMessage;
};
