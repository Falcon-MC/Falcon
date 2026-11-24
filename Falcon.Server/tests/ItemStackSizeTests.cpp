#include "World/Inventory/PlayerInventory.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ItemStack stack(const char *identifier, int count) {
        ItemStack item;
        item.mDefinition = std::make_shared<ItemDefinition>(identifier, 1, false, Tag());
        item.mCount = count;
        return item;
    }

    void inventoryUsesVanillaStackLimits() {
        struct StackLimit {
            const char *identifier;
            int maximum;
        };
        constexpr StackLimit limits[] = {
                {"minecraft:command_block_minecart", 1},
                {"minecraft:filled_map", 64},
                {"minecraft:glow_stick", 1},
                {"minecraft:ice_bomb", 16},
                {"minecraft:lodestone_compass", 64},
                {"minecraft:medicine", 1},
                {"minecraft:poplar_boat", 1},
                {"minecraft:poplar_chest_boat", 1},
                {"minecraft:poplar_hanging_sign", 16},
                {"minecraft:poplar_sign", 16},
                {"minecraft:sparkler", 1},
        };

        for (const StackLimit &limit: limits) {
            PlayerInventory inventory;
            std::vector<int> touchedSlots;
            require(inventory.addItem(stack(limit.identifier, limit.maximum + 1), touchedSlots),
                    limit.identifier);
            require(inventory.getItem(0).mCount == limit.maximum, limit.identifier);
            require(inventory.getItem(1).mCount == 1, limit.identifier);
        }
    }
}

int main() {
    try {
        inventoryUsesVanillaStackLimits();
        std::cout << "item stack size regressions passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
