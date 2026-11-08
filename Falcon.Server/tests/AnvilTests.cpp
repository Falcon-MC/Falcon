#include "Protocol/Types/ItemDefinitionRegistry.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Inventory/AnvilRepair.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/ItemData.h"
#include "World/Item/ItemEnchantments.h"

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

    ItemStack stack(const char *identifier, int count = 1, int damage = 0) {
        ItemStack item;
        item.mDefinition = std::make_shared<ItemDefinition>(identifier, 1, false, Tag());
        item.mCount = count;
        item.mDamage = damage;
        return item;
    }

    ItemStack enchanted(const char *identifier, const std::vector<EnchantmentInstance> &enchantments) {
        ItemStack item = stack(identifier);
        ItemEnchantments::write(item, enchantments);
        return item;
    }

    int32_t maxDamage(const char *identifier) {
        const ItemData *data = ItemDataTable::find(identifier);
        require(data != nullptr && data->mMaxDurability > 0, "missing durable item");
        return data->mMaxDurability;
    }

    AnvilResult combine(const ItemStack &input, const ItemStack &material, const std::string *name = nullptr,
                        bool creative = false) {
        return AnvilRepair::createResult(input, material, name, creative, nullptr);
    }

    void materialRepair() {
        const int32_t max = maxDamage("minecraft:diamond_sword");
        const AnvilResult result = combine(stack("minecraft:diamond_sword", 1, 1000), stack("minecraft:diamond", 2));
        require(!result.mResult.isAir(), "diamonds repair a diamond sword");
        require(result.mMaterialCost == 2 && result.mCost == 2, "a quarter of the durability per diamond");
        require(result.mResult.mDamage == 1000 - 2 * (max / 4), "each diamond restores a quarter");
        require(AnvilRepair::getRepairCost(result.mResult) == 1, "first use sets the penalty to 1");

        require(combine(stack("minecraft:diamond_sword"), stack("minecraft:diamond")).mResult.isAir(),
                "an undamaged item takes no material");
        require(combine(stack("minecraft:diamond_sword", 1, 100), stack("minecraft:iron_ingot")).mResult.isAir(),
                "the wrong material does nothing");
        require(!combine(stack("minecraft:wooden_pickaxe", 1, 30), stack("minecraft:cherry_planks")).mResult.isAir(),
                "every plank repairs the wooden tier");
        require(!combine(stack("minecraft:stone_axe", 1, 30), stack("minecraft:cobbled_deepslate")).mResult.isAir(),
                "cobbled deepslate repairs the stone tier");
        require(!combine(stack("minecraft:chainmail_helmet", 1, 30), stack("minecraft:iron_ingot")).mResult.isAir(),
                "chainmail takes iron");
        require(!combine(stack("minecraft:elytra", 1, 30), stack("minecraft:phantom_membrane")).mResult.isAir(),
                "elytra takes phantom membrane");
        require(!combine(stack("minecraft:mace", 1, 30), stack("minecraft:breeze_rod")).mResult.isAir(),
                "mace takes breeze rods");
        require(combine(stack("minecraft:bow", 1, 30), stack("minecraft:stick")).mResult.isAir(),
                "a bow has no repair material");

        const int32_t spearMax = maxDamage("minecraft:iron_spear");
        const AnvilResult spear = combine(stack("minecraft:iron_spear", 1, spearMax - 1), stack("minecraft:iron_ingot", 2));
        require(spear.mMaterialCost == 2 && spear.mResult.mDamage == spearMax - 1 - 2 * (int32_t) ((float) spearMax * 0.25f),
                "spears follow their repairable component");
    }

    void itemCombine() {
        const int32_t max = maxDamage("minecraft:iron_sword");
        const AnvilResult result = combine(stack("minecraft:iron_sword", 1, 200), stack("minecraft:iron_sword", 1, 100));
        const int32_t expected = max - ((max - 200) + (max - 100)) - 12 * max / 100;
        require(result.mResult.mDamage == expected, "two items add their durability plus 12%");
        require(result.mCost == 2 && result.mMaterialCost == 1, "combining durability costs two levels");

        require(combine(stack("minecraft:iron_sword", 1, 10), stack("minecraft:diamond_sword", 1, 10)).mResult.isAir(),
                "different items never combine");
        require(combine(stack("minecraft:iron_sword"), stack("minecraft:iron_sword")).mResult.isAir(),
                "two undamaged items without enchantments do nothing");
    }

    void enchantmentMerge() {
        const ItemStack sharpnessBook = enchanted("minecraft:enchanted_book", {{EnchantmentIds::SHARPNESS, 1}});
        const AnvilResult book = combine(stack("minecraft:diamond_sword"), sharpnessBook);
        require(ItemEnchantments::getLevel(book.mResult, EnchantmentIds::SHARPNESS) == 1, "the book adds sharpness");
        require(book.mCost == 1 && book.mMaterialCost == 1, "a common enchantment from a book costs one per level");

        const ItemStack sharp4 = enchanted("minecraft:diamond_sword", {{EnchantmentIds::SHARPNESS, 4}});
        const AnvilResult upgraded = combine(sharp4, sharp4);
        require(ItemEnchantments::getLevel(upgraded.mResult, EnchantmentIds::SHARPNESS) == 5, "equal levels go up one");
        require(upgraded.mCost == 1, "the cost is the value the result gained");

        const ItemStack sharp5 = enchanted("minecraft:diamond_sword", {{EnchantmentIds::SHARPNESS, 5}});
        require(combine(sharp5, enchanted("minecraft:enchanted_book", {{EnchantmentIds::SHARPNESS, 5}})).mResult.isAir(),
                "an enchantment at its maximum level is not raised");

        require(combine(sharp4, enchanted("minecraft:enchanted_book", {{EnchantmentIds::SMITE, 1}})).mResult.isAir(),
                "sharpness and smite conflict");
        require(combine(stack("minecraft:diamond_sword"), enchanted("minecraft:enchanted_book",
                                                                    {{EnchantmentIds::EFFICIENCY, 1}})).mResult.isAir(),
                "a sword cannot hold efficiency");

        const ItemStack silkPickaxe = enchanted("minecraft:diamond_pickaxe", {{EnchantmentIds::SILK_TOUCH, 1}});
        require(combine(stack("minecraft:diamond_pickaxe"), silkPickaxe).mCost == 8, "a very rare enchantment is worth 8");
        require(combine(stack("minecraft:diamond_pickaxe"),
                        enchanted("minecraft:enchanted_book", {{EnchantmentIds::SILK_TOUCH, 1}})).mCost == 4,
                "the book halves a very rare enchantment");

        const AnvilResult books = combine(enchanted("minecraft:enchanted_book", {{EnchantmentIds::PROTECTION, 1}}),
                                          enchanted("minecraft:enchanted_book", {{EnchantmentIds::PROTECTION, 1}}));
        require(ItemEnchantments::getLevel(books.mResult, EnchantmentIds::PROTECTION) == 2 && books.mCost == 1,
                "two books merge");

        const ItemStack loyal = enchanted("minecraft:trident", {{EnchantmentIds::LOYALTY, 1}});
        require(!combine(loyal, enchanted("minecraft:enchanted_book", {{EnchantmentIds::CHANNELING, 1}})).mResult.isAir(),
                "loyalty and channeling go together");
        const ItemStack riptide = enchanted("minecraft:trident", {{EnchantmentIds::RIPTIDE, 1}});
        require(combine(riptide, enchanted("minecraft:enchanted_book", {{EnchantmentIds::CHANNELING, 1}})).mResult.isAir(),
                "riptide refuses channeling");
        require(combine(riptide, enchanted("minecraft:enchanted_book", {{EnchantmentIds::LOYALTY, 1}})).mResult.isAir(),
                "riptide refuses loyalty");

        const ItemStack smiteMace = enchanted("minecraft:mace", {{EnchantmentIds::SMITE, 1}});
        require(combine(smiteMace, enchanted("minecraft:enchanted_book", {{EnchantmentIds::DENSITY, 1}})).mResult.isAir(),
                "density conflicts with smite");

        ItemStack pair = stack("minecraft:enchanted_book", 2);
        ItemEnchantments::write(pair, {{EnchantmentIds::UNBREAKING, 1}});
        require(combine(pair, enchanted("minecraft:enchanted_book", {{EnchantmentIds::MENDING, 1}})).mResult.isAir(),
                "enchanting a stack is too expensive");
    }

    void renaming() {
        const std::string name = "Foo";
        const AnvilResult renamed = combine(stack("minecraft:diamond_sword"), ItemStack::air(), &name);
        require(AnvilRepair::hasCustomName(renamed.mResult) && renamed.mCost == 1, "renaming costs one level");
        require(AnvilRepair::getRepairCost(renamed.mResult) == 0, "renaming alone keeps the penalty");

        const std::string same = "Diamond Sword";
        require(combine(stack("minecraft:diamond_sword"), ItemStack::air(), &same).mResult.isAir(),
                "keeping the name changes nothing");

        const std::string cleared;
        const AnvilResult unnamed = combine(renamed.mResult, ItemStack::air(), &cleared);
        require(!AnvilRepair::hasCustomName(unnamed.mResult) && unnamed.mCost == 1, "clearing the name costs one");

        require(combine(stack("minecraft:diamond_sword"), ItemStack::air()).mResult.isAir(),
                "an empty material with no name makes nothing");

        ItemStack worn = stack("minecraft:diamond_sword");
        worn.mTag = Tag::ofCompound();
        worn.mTag.putInt(AnvilRepair::TAG_REPAIR_COST, 45);
        const AnvilResult capped = combine(worn, ItemStack::air(), &name);
        require(!capped.mResult.isAir() && capped.mCost == 39, "renaming alone is capped below too expensive");
    }

    void penalties() {
        ItemStack worn = stack("minecraft:diamond_sword", 1, 500);
        worn.mTag = Tag::ofCompound();
        worn.mTag.putInt(AnvilRepair::TAG_REPAIR_COST, 3);
        const AnvilResult repaired = combine(worn, stack("minecraft:diamond"));
        require(repaired.mCost == 4, "the penalty adds to the cost");
        require(AnvilRepair::getRepairCost(repaired.mResult) == 7, "the penalty doubles plus one");

        worn.mTag.putInt(AnvilRepair::TAG_REPAIR_COST, 39);
        require(combine(worn, stack("minecraft:diamond")).mResult.isAir(), "40 levels is too expensive");
        const AnvilResult creative = combine(worn, stack("minecraft:diamond"), nullptr, true);
        require(!creative.mResult.isAir() && creative.mCost == 40, "creative ignores too expensive");
    }

    // a behavior pack item repaired through its minecraft:repairable entries
    void customRepair() {
        CustomItemDefinition blade;
        blade.mIdentifier = "test:ruby_blade";
        blade.mMaxStackSize = 1;
        blade.mMaxDurability = 400;
        blade.mRepairItems.push_back({{"test:ruby"}, "q.max_durability * 0.25"});
        blade.mRepairItems.push_back({{"test:ruby_blade"},
                                      "context.other->query.remaining_durability + 0.05 * context.other->query.max_durability"});
        ItemDefinitionRegistry items;
        require(CustomContentRegistry::getInstance().registerItem(blade, items) != nullptr, "custom item registers");

        const AnvilResult rubies = combine(stack("test:ruby_blade", 1, 300), stack("test:ruby", 3));
        require(!rubies.mResult.isAir(), "rubies repair the custom blade");
        require(rubies.mMaterialCost == 3 && rubies.mCost == 3, "each ruby restores a quarter");
        require(rubies.mResult.mDamage == 0, "three rubies restore 300 damage");

        const AnvilResult two = combine(stack("test:ruby_blade", 1, 300), stack("test:ruby_blade", 1, 300));
        require(!two.mResult.isAir() && two.mResult.mDamage < 300, "a second blade repairs the first");

        require(combine(stack("test:ruby_blade", 1, 300), stack("minecraft:diamond")).mResult.isAir(),
                "an unlisted material does nothing");
    }

    void enchantValues() {
        const std::vector<EnchantmentInstance> list = {{EnchantmentIds::SHARPNESS, 5}, {EnchantmentIds::UNBREAKING, 3},
                                                       {EnchantmentIds::FORTUNE, 2}, {EnchantmentIds::THORNS, 1}};
        require(AnvilRepair::getEnchantValue(list, false) == 5 * 1 + 3 * 2 + 2 * 4 + 1 * 8, "item weights");
        require(AnvilRepair::getEnchantValue(list, true) == 5 * 1 + 3 * 1 + 2 * 2 + 1 * 4, "book weights");
    }
}

int main() {
    try {
        materialRepair();
        itemCombine();
        enchantmentMerge();
        renaming();
        penalties();
        enchantValues();
        customRepair();
        std::cout << "anvil repair, merge and rename checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
