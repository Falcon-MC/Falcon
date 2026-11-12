#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class CauldronBlock final : public Block {
public:
    enum class Liquid { Empty, Water, Lava, PowderSnow };

    explicit CauldronBlock(const BlockState &state) : Block(state) {}

    explicit CauldronBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    int getFillLevel() const;

    Liquid getLiquid() const;

    bool isFull() const;

    bool canFill(Liquid liquid) const;

    BlockState withFillLevel(Liquid liquid, int fillLevel) const;

    static BlockState makeState(Liquid liquid, int fillLevel);

    int getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const override;

    // dyes the water, or dyes or washes a dyeable item in it
    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;
};
