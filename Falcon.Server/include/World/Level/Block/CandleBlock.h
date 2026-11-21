#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class Actor;

class CandleBlock final : public Block {
public:
    explicit CandleBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    PlacementMergeResult mergePlacement(Level &level, const Vector3i &clickedPosition, int blockFace,
                                        const Vector3f &clickPosition, Vector3i &position,
                                        BlockState &state) const override;

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    // flint and steel, or any item with Fire Aspect but a book
    static bool isLighter(const ItemStack &item);

    static bool canBeLit(Level &level, const Vector3i &position, const BlockState &state);

    // lit with the ignite sound; false when it can't be lit
    static bool light(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                      Actor *source);

    static void extinguish(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state, Actor *source);
};
