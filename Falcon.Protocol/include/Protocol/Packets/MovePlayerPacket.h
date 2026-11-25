#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Packet.h"

#include <cstdint>

enum class MovePlayerMode {
    Normal,
    Respawn,
    Teleport,
    HeadRotation,
};

enum class MovePlayerTeleportationCause {
    Unknown,
    Projectile,
    ChorusFruit,
    Command,
    Behavior,
};

class MovePlayerPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::MovePlayer;

    MovePlayerPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "MovePlayerPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mRuntimeActorId = 0;
    Vector3f mPosition;
    Vector3f mRotation;
    MovePlayerMode mMode = MovePlayerMode::Normal;
    bool mOnGround = false;
    int64_t mRidingRuntimeActorId = 0;
    bool mHasTeleportData = false;
    MovePlayerTeleportationCause mTeleportationCause = MovePlayerTeleportationCause::Unknown;
    int32_t mActorType = 0;
    int64_t mTick = 0;
};
