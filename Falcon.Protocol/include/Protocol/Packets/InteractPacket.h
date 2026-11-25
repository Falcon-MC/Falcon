#pragma once

#include "Protocol/Packet.h"

class InteractPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::Interact;

    enum class Action : int {
        None,
        Interact,
        Damage,
        LeaveVehicle,
        MouseOver,
        NpcOpen,
        OpenInventory
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
