#pragma once

#include "Loot/LootContext.h"
#include "Protocol/Types/ItemStack.h"

#include <random>
#include <vector>

class Container;
class ServerNetworkHandler;

class LootItems {
public:
    static ItemStack toItemStack(ServerNetworkHandler &owner, const LootDrop &drop);

    static void fillContainer(ServerNetworkHandler &owner, Container &container, const std::vector<LootDrop> &drops,
                              std::mt19937 &random);
};
