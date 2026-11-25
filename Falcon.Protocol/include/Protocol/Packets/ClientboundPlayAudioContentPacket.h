#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Packets/PlaySoundPacket.h"

#include <string>

class ClientboundPlayAudioContentPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ClientboundPlayAudioContent;

    ClientboundPlayAudioContentPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ClientboundPlayAudioContentPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mSharedMetadata;
    std::string mPlaybackContent;
    std::string mPlaybackType;
    PlaySoundPacket mSound;
};
