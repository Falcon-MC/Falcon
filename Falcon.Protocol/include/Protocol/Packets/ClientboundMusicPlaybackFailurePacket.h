#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/MusicPlaybackTypes.h"

#include <vector>

class ClientboundMusicPlaybackFailurePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ClientboundMusicPlaybackFailure;

    ClientboundMusicPlaybackFailurePacket();

    MinecraftPacketIds getId() const override {
        return ID;
    }

    const char *getName() const override {
        return "ClientboundMusicPlaybackFailurePacket";
    }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<MusicPlaybackFailureEntry> mFailures;
};
