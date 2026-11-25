#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

class ServerboundCursorItemDragPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ServerboundCursorItemDrag;

    enum class State : uint8_t {
        Start = 0,
        Stop = 1
    };

    ServerboundCursorItemDragPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ServerboundCursorItemDragPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    State mState = State::Start;
};
