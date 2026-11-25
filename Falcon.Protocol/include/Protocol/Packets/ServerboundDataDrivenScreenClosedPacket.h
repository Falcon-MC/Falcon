#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

class ServerboundDataDrivenScreenClosedPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ServerboundDataDrivenScreenClosed;

    enum class CloseReason {
        ProgrammaticClose,
        ProgrammaticCloseAll,
        ClientCanceled,
        UserBusy,
        InvalidForm,
    };

    ServerboundDataDrivenScreenClosedPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ServerboundDataDrivenScreenClosedPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int32_t mFormId = 0;
    CloseReason mCloseReason = CloseReason::ProgrammaticClose;
};
