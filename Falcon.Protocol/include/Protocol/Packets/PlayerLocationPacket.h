#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Packet.h"

#include <cstdint>

enum class PlayerLocationType {
    Coordinates,
    Hide,
};

class PlayerLocationPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PlayerLocation;

    PlayerLocationPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PlayerLocationPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mTargetActorId = 0;
    PlayerLocationType mType = PlayerLocationType::Hide;
    Vector3f mPosition;
};
