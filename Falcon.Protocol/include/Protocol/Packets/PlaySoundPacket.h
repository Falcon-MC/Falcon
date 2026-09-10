#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

class PlaySoundPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PlaySound;

    PlaySoundPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PlaySoundPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mSound;
    Vector3f mPosition;
    float mVolume = 0.0f;
    float mPitch = 0.0f;
    int32_t mLoopCount = 0;
    bool mBypassListenerRangeCheck = false;
    bool mHasServerSoundHandle = false;
    int64_t mServerSoundHandle = 0;
    bool mHasPlaybackPosition = false;
    float mPlaybackPositionSeconds = 0.0f;
};
