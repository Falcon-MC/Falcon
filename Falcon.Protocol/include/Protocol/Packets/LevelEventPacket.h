#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Packet.h"

class LevelEventPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::LevelEvent;

    enum Event : int32_t {
        SoundGhastFireball = 1008,
        SoundItemFrameAddItem = 1040,
        SoundItemFrameBreak = 1041,
        SoundItemFramePlace = 1042,
        SoundItemFrameRemoveItem = 1043,
        SoundItemFrameRotateItem = 1044,
        SoundTotemUsed = 1052,
        ParticlePunchBlock = 2014,
        BlockStartBreak = 3600,
        BlockStopBreak = 3601,
        BlockUpdateBreak = 3602,
        ParticleDestroy = 2001,
        ParticleSplash = 2002,
        ParticleExplode = 2025,
        ParticleSmashAttackGroundDust = 9815,
        StartRain = 3001,
        StartThunder = 3002,
        StopRain = 3003,
        StopThunder = 3004
    };

    LevelEventPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "LevelEventPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int32_t mEventId;
    Vector3f mPosition;
    int32_t mData;

    /**
     * For ParticlePunchBlock, the hit face, which the packet packs into the top byte of the block network ID once
     * that ID is translated for the client. mData then holds the block alone. It is not part of the packet.
     */
    int32_t mBlockFace = 0;
};
