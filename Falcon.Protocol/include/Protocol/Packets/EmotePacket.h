#pragma once

#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class EmotePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::Emote;

    static const uint8_t FlagServerSide = 1 << 0;
    static const uint8_t FlagMuteEmoteChat = 1 << 1;

    EmotePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "EmotePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint64_t mRuntimeActorId = 0;
    std::string mEmoteId;
    uint32_t mEmoteDuration = 0;
    std::string mXuid;
    std::string mPlatformId;
    uint8_t mFlags = 0;
};
