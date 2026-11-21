#include "World/Inventory/GrindstoneRepair.h"

#include "World/Inventory/AnvilRepair.h"
#include "World/Item/EnchantmentHelper.h"
#include "World/Item/Item.h"
#include "World/Item/VanillaItems.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace {
    int32_t maxDamage(const std::string &identifier) {
        const Item *item = VanillaItems::fromIdentifier(identifier);
        return item == nullptr ? 0 : item->getMaxDurability();
    }
}

GrindstoneResult GrindstoneRepair::createResult(const ItemStack &input, const ItemStack &additional,
                                               const std::shared_ptr<ItemDefinition> &book) {
    if ((input.isAir() && additional.isAir()) || (!input.isAir() && input.mCount != 1)
        || (!additional.isAir() && additional.mCount != 1))
        return {};

    const ItemStack &base = input.isAir() ? additional : input;
    const std::string &identifier = base.mDefinition->getIdentifier();
    const bool combining = !input.isAir() && !additional.isAir();
    ItemStack result = base;
    if (combining) {
        const int64_t durability = maxDamage(identifier);
        if (identifier != additional.mDefinition->getIdentifier() || durability <= 0
            || input.mTag.getBool("Unbreakable", false) || additional.mTag.getBool("Unbreakable", false))
            return {};

        if (!input.mTag.isCompound() && additional.mTag.isCompound())
            result = additional;

        const int64_t first = std::clamp<int64_t>(input.mDamage, 0, durability);
        const int64_t second = std::clamp<int64_t>(additional.mDamage, 0, durability);
        result.mDamage = (int32_t) std::max<int64_t>(0, first + second - durability - durability / 20);
    }

    std::vector<EnchantmentInstance> curses;
    int32_t experienceCost = 0;
    for (const ItemStack *item: {&input, &additional}) {
        if (item->isAir())
            continue;
        std::unordered_set<int32_t> counted;
        for (const EnchantmentInstance &enchantment: ItemEnchantments::read(*item)) {
            if (enchantment.mId == EnchantmentIds::BINDING || enchantment.mId == EnchantmentIds::VANISHING) {
                const bool alreadyKept = std::any_of(curses.begin(), curses.end(), [&](const auto &curse) {
                    return curse.mId == enchantment.mId && curse.mLevel == enchantment.mLevel;
                });
                if (!alreadyKept)
                    curses.push_back(enchantment);
            } else if (const auto *definition = EnchantmentTable::findById(enchantment.mId);
                       definition != nullptr && counted.insert(enchantment.mId).second) {
                const int32_t level = std::min(enchantment.mLevel, definition->mMaxLevel);
                experienceCost += EnchantmentHelper::getMinAbility(enchantment.mId, level);
            }
        }
    }

    if (curses.size() > 3)
        return {};
    static constexpr int32_t repairCosts[] = {0, 1, 3, 0};
    if (result.mTag.isCompound()) {
        ItemEnchantments::write(result, curses);
        if (combining)
            result.mTag.remove("Damage");
        const int32_t repairCost = repairCosts[curses.size()];
        if (repairCost == 0)
            result.mTag.remove(AnvilRepair::TAG_REPAIR_COST);
        else
            result.mTag.putInt(AnvilRepair::TAG_REPAIR_COST, repairCost);
        if (result.mTag.isEmpty())
            result.mTag = Tag();
    }

    if (identifier == "minecraft:enchanted_book" && curses.empty()) {
        if (book == nullptr)
            return {};
        result = ItemStack::air();
        result.mDefinition = book;
    } else if (!combining && result.mTag == base.mTag) {
        return {};
    }

    result.mCount = 1;
    result.mUserData.clear();
    result.mUsingNetId = false;
    result.mNetId = 0;
    return {std::move(result), experienceCost};
}
