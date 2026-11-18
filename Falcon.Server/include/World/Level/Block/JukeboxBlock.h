#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class JukeboxBlock : public Block {
public:
    explicit JukeboxBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    const char *getBlockActorId() const override;

    // a record comes out, or the held record goes in and plays
    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, const ItemStack &usedItem, int blockFace) const override;

    int getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const override;

    bool isSignalSource() const override {
        return true;
    }

    // full power around it while a record plays, never through a block
    int getEmittedSignal(Level &level, const Vector3i &position, const BlockState &state, int face,
                         bool strong) const override;
};
