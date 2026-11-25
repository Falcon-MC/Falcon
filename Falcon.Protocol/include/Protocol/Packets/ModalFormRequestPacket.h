#pragma once

#include "Protocol/Packet.h"

class ModalFormRequestPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ModalFormRequest;

    ModalFormRequestPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ModalFormRequestPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint32_t mFormId;
    std::string mFormData;
};
