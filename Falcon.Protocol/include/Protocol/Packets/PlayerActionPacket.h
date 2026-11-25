#pragma once

#include "Core/Math/Vector3i.h"
#include "Protocol/Packet.h"
#include "Protocol/Types/PlayerBlockActionData.h"

#include <cstdint>

class PlayerActionPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PlayerAction;

    PlayerActionPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PlayerActionPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mRuntimeActorId = 0;
    PlayerActionType mAction = PlayerActionType::StartBreak;
    Vector3i mBlockPosition;
    Vector3i mResultPosition;
    int32_t mFace = 0;
};
