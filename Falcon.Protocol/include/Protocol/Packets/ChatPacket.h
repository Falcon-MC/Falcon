#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/ChatTypes.h"

class ChatPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::Chat;

    ChatPacket();

    MinecraftPacketIds getId() const override {
        return ID;
    }

    const char *getName() const override {
        return "ChatPacket";
    }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    SignedChat mSignedChat;
    bool mHasRoomId = false;
    std::string mRoomId;
    bool mHasFormatting = false;
    ChatFormatting mFormatting;
};
