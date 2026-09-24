#pragma once

#include "Block/Block.h"

class BuddingAmethystBlock : public Block {
public:
    explicit BuddingAmethystBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};

class PointedDripstoneBlock : public Block {
public:
    explicit PointedDripstoneBlock(const Block &block) : Block(block) {
    }

    static constexpr const char *IDENTIFIER = "minecraft:pointed_dripstone";

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

private:
    static bool isHanging(const BlockState &state);

    static bool pointsTowards(const BlockState &state, bool hanging);

    static bool findTip(Level &level, const Vector3i &root, bool hanging, Vector3i &tip);

    static bool canTipGrow(Level &level, const Vector3i &tip, bool hanging);

    static void grow(Level &level, const Vector3i &tip, bool hanging);

    static void growStalagmiteBelow(Level &level, const Vector3i &tip);

    static void refreshThickness(Level &level, const Vector3i &position, bool hanging);
};
