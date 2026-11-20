#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class ItemStack;
class ServerNetworkHandler;
class ServerPlayer;

class ComposterBlock final : public Block {
public:
    explicit ComposterBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    int getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const override;

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    // a full composter turns ready
    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;

    // out of 100: the item's minecraft:compostable component, else the game's table; 0 for anything else
    static int32_t compostChance(const ItemStack &item);

    // up to count of the stack into the composter, the items it used; 0: not compostable, or full
    static int32_t addItems(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const ItemStack &item,
                            int32_t count);

    // one item; false when it takes none
    static bool addItem(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const ItemStack &item);

    static bool isReady(const BlockState &state);

    // a full composter empties for its bone meal; false when it is not full
    static bool takeOutput(ServerNetworkHandler &owner, Level &level, const Vector3i &position);
};
