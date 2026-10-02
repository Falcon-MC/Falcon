#pragma once

#include "Block/Block.h"

#include <cstdint>
#include <string>

class BeehiveBlock final : public Block {
public:
    static constexpr int32_t MAX_HONEY_LEVEL = 5;

    explicit BeehiveBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    static int32_t getHoneyLevel(const BlockState &state);

    static void setHoneyLevel(Level &level, const Vector3i &position, int32_t honeyLevel);

    static int getFrontFace(const BlockState &state);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, const ItemStack &usedItem, int blockFace) const override;

    void onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                  const BlockState &state) const override;

    bool onActorEvent(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                      const std::string &event, MobActor &source) const override;

    void writeDropContents(Level &level, const Vector3i &position, ItemStack &drop) const override;
};
