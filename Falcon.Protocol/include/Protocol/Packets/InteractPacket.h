#pragma once

#include "Protocol/Packet.h"

class InteractPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::Interact;

    // the game's values, sent as one byte: 1 and 2 are no longer used
    enum class Action : uint8_t {
        Invalid = 0,
        StopRiding = 3,
        InteractUpdate = 4,
        NpcOpen = 5,
        OpenInventory = 6
    };

    InteractPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "InteractPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Action mAction;
    uint64_t mRuntimeActorId;
    bool mHasMousePosition;
    Vector3f mMousePosition;
};
