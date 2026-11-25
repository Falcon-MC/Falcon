#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Packet.h"

#include <cstdint>
#include <string>

namespace PlaySoundName {
    inline constexpr const char *SHIELD_BLOCK = "item.shield.block";
    inline constexpr const char *FUSE = "random.fuse";
    inline constexpr const char *FIRE_IGNITE = "fire.ignite";
    inline constexpr const char *TURTLE_EGG_CRACK = "block.turtle_egg.crack";
    inline constexpr const char *SNIFFER_EGG_CRACK = "block.sniffer_egg.crack";
    inline constexpr const char *SNIFFER_EGG_HATCH = "block.sniffer_egg.hatch";
    inline constexpr const char *ANVIL_LAND = "random.anvil_land";
    inline constexpr const char *POINTED_DRIPSTONE_LAND = "pointed_dripstone.land";
    inline constexpr const char *ORB = "random.orb";
    inline constexpr const char *END_PORTAL_FRAME_FILL = "block.end_portal_frame.fill";
    inline constexpr const char *END_PORTAL_SPAWN = "block.end_portal.spawn";
}

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
