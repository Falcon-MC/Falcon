#pragma once

#include "World/Level/Storage/Loot/LootContext.h"
#include "Protocol/Types/ItemStack.h"

#include <random>
#include <vector>

class Container;
class ServerNetworkHandler;

class LootItems {
public:
    struct PlacedDrop {
        int32_t mSlot = 0;
        LootDrop mDrop;
    };

    static ItemStack toItemStack(ServerNetworkHandler &owner, const LootDrop &drop);

    // the game's draw order: stacks split, then items and slots shuffle
    static std::vector<PlacedDrop> distribute(std::vector<LootDrop> drops, std::vector<int32_t> freeSlots,
                                              std::mt19937 &random);

    static void fillContainer(ServerNetworkHandler &owner, Container &container, const std::vector<LootDrop> &drops,
                              std::mt19937 &random);
};
