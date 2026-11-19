#pragma once

#include "World/Level/Block/Actor/BlockActor.h"

#include <cstdint>
#include <vector>

class Level;

/**
 * The block actor of a conduit. Every 40 ticks it checks the water around it and counts its prismarine frame:
 * 16 frame blocks give conduit power to the players in range, 42 make it attack the monsters next to it.
 */
class ConduitBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Conduit";

    static constexpr int32_t UPDATE_TICKS = 40;

    // frame blocks needed to give power, and to hunt
    static constexpr int32_t ACTIVE_FRAME = 16;
    static constexpr int32_t HUNTING_FRAME = 42;

    static constexpr int32_t EFFECT_TICKS = 260;

    static constexpr float ATTACK_DAMAGE = 4.0f;

    static constexpr int64_t NO_TARGET = -1;

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    Tag getSpawnCompound() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    bool tick(ServerNetworkHandler &owner) override;

    bool isActive() const noexcept {
        return mActive;
    }

    bool isHunting() const noexcept {
        return mHunting;
    }

    int64_t getTarget() const noexcept {
        return mTarget;
    }

    int32_t getEffectRange() const noexcept {
        return mEffectRange;
    }

    // the 26 blocks around the conduit are all water; an unloaded one is not
    static bool isSurroundedByWater(Level &level, const Vector3i &position);

    // the frame blocks on the three rings of radius 2 around the conduit, in the game's order
    static void collectFrame(Level &level, const Vector3i &position, std::vector<Vector3i> &outFrame);

    static bool isFrameBlock(const BlockState &state);

    // 16 blocks per 7 frame blocks
    static int32_t rangeFor(int32_t frameBlocks) {
        return 16 * (frameBlocks / 7);
    }

private:
    void _update(ServerNetworkHandler &owner, Level &level);

    void _applyEffects(ServerNetworkHandler &owner, Level &level);

    void _attack(ServerNetworkHandler &owner, Level &level);

    std::vector<Vector3i> mFrame;
    int64_t mTarget = NO_TARGET;
    int64_t mNextShortSoundTick = 0;
    int32_t mCountdown = 0;
    int32_t mEffectRange = 0;
    bool mActive = false;
    bool mHunting = false;
};
