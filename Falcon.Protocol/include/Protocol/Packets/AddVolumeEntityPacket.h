#pragma once

#include "Core/Math/Vector3i.h"
#include "Core/NBT/Tag.h"
#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class AddVolumeEntityPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::AddVolumeEntity;

    AddVolumeEntityPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "AddVolumeEntityPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint32_t mId = 0;
    Tag mData;
    std::string mIdentifier;
    std::string mInstanceName;
    Vector3i mMinBounds;
    Vector3i mMaxBounds;
    int32_t mDimension = 0;
    std::string mEngineVersion;
};
