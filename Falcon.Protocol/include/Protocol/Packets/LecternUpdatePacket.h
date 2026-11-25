#pragma once

#include "Core/Math/Vector3i.h"
#include "Protocol/Packet.h"

class LecternUpdatePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::LecternUpdate;

    MinecraftPacketIds getId() const override
    {
        return ID;
    }

    const char* getName() const override
    {
        return "LecternUpdatePacket";
    }

    void write(BinaryStream& stream, const PacketCodecContext& context) const override;
    void read(ReadOnlyBinaryStream& stream, const PacketCodecContext& context) override;
    void handle(const NetworkIdentifier& id, NetworkPacketHandler& handler) const override;

    uint8_t mPage = 0;
    uint8_t mTotalPages = 0;
    Vector3i mBlockPosition;
};
