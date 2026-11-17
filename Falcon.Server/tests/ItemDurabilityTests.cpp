#include "World/Item/ItemData.h"
#include "World/Item/ItemDurability.h"

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

    void vanillaDurabilityMatchesItemProperties() {
        struct ExpectedDurability {
            const char *identifier;
            int maxDurability;
        };
        constexpr ExpectedDurability expected[] = {
                {"minecraft:bow", 384},
                {"minecraft:copper_axe", 190},
                {"minecraft:copper_nautilus_armor", 0},
                {"minecraft:copper_spear", 190},
                {"minecraft:diamond_spear", 1560},
                {"minecraft:diamond_sword", 1561},
                {"minecraft:glow_stick", 100},
                {"minecraft:golden_spear", 30},
                {"minecraft:iron_sword", 250},
                {"minecraft:netherite_spear", 2030},
                {"minecraft:shield", 336},
                {"minecraft:sparkler", 100},
                {"minecraft:stone_spear", 130},
                {"minecraft:wooden_sword", 59},
        };

        for (const ExpectedDurability &value: expected) {
            const ItemData *data = ItemDataTable::find(value.identifier);
            require(data != nullptr, std::string(value.identifier) + " is missing from the item table");
            require(data->mMaxDurability == value.maxDurability,
                    std::string(value.identifier) + " has the wrong maximum durability");

            const DurabilityItemComponent *component =
                    ItemDataTable::getComponents(value.identifier).get<DurabilityItemComponent>();
            require((component != nullptr) == (value.maxDurability > 0),
                    std::string(value.identifier) + " has the wrong durability component presence");
            if (component != nullptr)
                require(component->getMaxDurability() == value.maxDurability,
                        std::string(value.identifier) + " exposes the wrong durability component value");
        }
    }

    void ironSwordBreaksOnItsLastDurabilityPoint() {
        ItemStack sword = stack("minecraft:iron_sword");
        for (int hit = 0; hit < 249; ++hit)
            require(ItemDurability::apply(nullptr, sword, 1, false) && !sword.isAir(),
                    "the iron sword broke before using 249 durability points");

        require(sword.mDamage == 249, "the iron sword did not retain its current damage");
        require(ItemDurability::apply(nullptr, sword, 1, false) && sword.isAir(),
                "the iron sword survived past its 250th durability point");
    }
}

int main() {
    try {
        vanillaDurabilityMatchesItemProperties();
        ironSwordBreaksOnItsLastDurabilityPoint();
        std::cout << "item durability regressions passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
