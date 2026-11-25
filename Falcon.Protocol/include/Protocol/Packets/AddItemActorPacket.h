#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Packet.h"
#include "Protocol/Types/EntityDataMap.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>

class AddItemActorPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::AddItemActor;

    AddItemActorPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "AddItemActorPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mUniqueActorId = 0;
    uint64_t mRuntimeActorId = 0;
    ItemStack mItemInHand;
    Vector3f mPosition;
    Vector3f mMotion;
    EntityDataMap mMetadata;
    bool mFromFishing = false;
};
