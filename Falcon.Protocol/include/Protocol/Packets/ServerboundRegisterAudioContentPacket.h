#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/AudioContentRegistrationEntry.h"

#include <utility>
#include <vector>

class ServerboundRegisterAudioContentPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ServerboundRegisterAudioContent;

    ServerboundRegisterAudioContentPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ServerboundRegisterAudioContentPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<AudioContentRegistrationEntry> mRegistrations;
};
