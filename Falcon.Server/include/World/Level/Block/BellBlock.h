#pragma once

#include "World/Level/Block/Block.h"

#include <cstdint>
#include <string>

class Actor;

class BellBlock final : public Block {
public:
    explicit BellBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    const char *getBlockActorId() const override;

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;

    // rings when the player faces the right way for its attachment
    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    // the hit that ringing makes: the swing, the sound, the toggled state and the mobs around sent hiding
    static void ring(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                     int32_t direction, Actor *source);

    bool onProjectileHit(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                         ServerActor &projectile) const override;

    // a thrown item bounces off it, away from its middle, and rings it the way it faces
    bool onItemHit(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                   ItemActor &item) const override;

    // rings as the power comes on
    void onRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         const BlockState &state) const override;

    bool onWindBurst(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                     const BlockState &state) const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position, const BlockState &state,
                  const ItemStack &usedItem, int blockFace) const override;
};
