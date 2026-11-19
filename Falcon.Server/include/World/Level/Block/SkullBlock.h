#pragma once

#include "World/Level/Block/FaceAttachedBlock.h"

#include <string>

class SkullBlock : public FaceAttachedBlock {
public:
    explicit SkullBlock(const Block &block) : FaceAttachedBlock(block) {}

    static bool matches(const std::string &identifier);

    PistonMoveReaction getPistonMoveReaction() const override;

    bool isWornOnHead() const override {
        return true;
    }

    const char *getBlockActorId() const override;

    // a skull put on a floor faces the player, in 16 steps
    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, const ItemStack &usedItem, int blockFace) const override;
};

// placing it may complete the T of soul blocks that summons the wither
class WitherSkeletonSkullBlock final : public SkullBlock {
public:
    explicit WitherSkeletonSkullBlock(const Block &block) : SkullBlock(block) {}

    static bool matches(const std::string &identifier);

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, const ItemStack &usedItem, int blockFace) const override;

    // placed in front of the dispenser only where it may complete the T; dropped otherwise
    bool dispense(const DispenseSource &source) const override;
};
