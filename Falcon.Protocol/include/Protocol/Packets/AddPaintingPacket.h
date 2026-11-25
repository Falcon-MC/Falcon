#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class AddPaintingPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::AddPainting;

    AddPaintingPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "AddPaintingPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mUniqueActorId = 0;
    uint64_t mRuntimeActorId = 0;
    Vector3f mPosition;
    int32_t mDirection = 0;
    std::string mMotive;
};
