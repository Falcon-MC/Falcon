#pragma once

#include "Block/Block.h"

#include <string>

class ChestBlockActor;
class Level;

class ChestBlock : public Block {
public:
    explicit ChestBlock(const Block &base);

    static bool matches(const std::string &identifier);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, const ItemStack &usedItem, int blockFace) const override;

    void onBroken(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state) const override;

    static ChestBlockActor &getOrCreate(Level &level, const Vector3i &position);

    static void pair(Level &level, const Vector3i &position);
};
