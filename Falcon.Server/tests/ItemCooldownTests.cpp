#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/ItemData.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ItemStack stack(const char *identifier) {
        ItemStack item;
        item.mDefinition = std::make_shared<ItemDefinition>(identifier, 1, false, Tag());
        item.mCount = 1;
        return item;
    }

    void cooldownMetadataMatchesVanilla() {
        struct Cooldown {
            const char *identifier;
            const char *category;
            int ticks;
        };
        constexpr Cooldown cooldowns[] = {
                {"minecraft:chorus_fruit", "minecraft:chorusfruit", 20},
                {"minecraft:copper_spear", "minecraft:spear", 17},
                {"minecraft:diamond_spear", "minecraft:spear", 21},
                {"minecraft:ender_pearl", "minecraft:ender_pearl", 20},
                {"minecraft:goat_horn", "minecraft:goat_horn", 140},
                {"minecraft:golden_spear", "minecraft:spear", 19},
                {"minecraft:ice_bomb", "minecraft:ice_bomb", 10},
                {"minecraft:iron_spear", "minecraft:spear", 19},
                {"minecraft:netherite_spear", "minecraft:spear", 23},
                {"minecraft:shield", "minecraft:shield", 100},
                {"minecraft:stone_spear", "minecraft:spear", 15},
                {"minecraft:wind_charge", "minecraft:wind_charge", 10},
                {"minecraft:wooden_spear", "minecraft:spear", 13},
        };

        ServerPlayer player(NetworkIdentifier{}, 100);
        for (const Cooldown &expected: cooldowns) {
            const CooldownItemComponent *component =
                    ItemDataTable::getComponents(expected.identifier).get<CooldownItemComponent>();
            require(component != nullptr, std::string(expected.identifier) + " is missing its cooldown component");
            require(component->getCategory() == expected.category,
                    std::string(expected.identifier) + " has the wrong cooldown category");
            require(component->getDurationTicks() == expected.ticks,
                    std::string(expected.identifier) + " has the wrong cooldown duration");

            const ItemStack item = stack(expected.identifier);
            player.startItemCooldown(item, 100);
            require(player.getItemCooldownRemaining(item, 100) == expected.ticks,
                    std::string(expected.identifier) + " did not start its vanilla cooldown");
            require(player.getItemCooldownRemaining(item, 100 + expected.ticks) == 0,
                    std::string(expected.identifier) + " remained on cooldown after it expired");
        }
    }

    void spearTiersShareCooldownCategory() {
        ServerPlayer player(NetworkIdentifier{}, 101);
        const ItemStack copper = stack("minecraft:copper_spear");
        const ItemStack diamond = stack("minecraft:diamond_spear");

        player.startItemCooldown(copper, 200);
        require(player.getItemCooldownRemaining(diamond, 200) == 17,
                "a cooldown from one spear tier did not block another tier");
        require(player.getItemCooldownRemaining(diamond, 217) == 0,
                "the shared spear cooldown did not expire at the copper spear duration");
    }
}

int main() {
    try {
        cooldownMetadataMatchesVanilla();
        spearTiersShareCooldownCategory();
        std::cout << "item cooldown regressions passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
