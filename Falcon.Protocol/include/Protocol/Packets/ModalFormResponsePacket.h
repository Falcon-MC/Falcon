#pragma once

#include "Protocol/Packet.h"

class ModalFormResponsePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ModalFormResponse;

    enum class CancelReason : int {
        UserClosed,
        UserBusy
    };

    ModalFormResponsePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ModalFormResponsePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint32_t mFormId;
    bool mHasFormData;
    std::string mFormData;
    bool mHasCancelReason;
    CancelReason mCancelReason;
};
