#include "Loot/LootItems.h"

#include "Inventory/Container.h"
#include "Item/ItemData.h"
#include "Loot/LegacyItemMapper.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <cstddef>
#include <string>

namespace {
    const char *BOOK = "minecraft:book";
    const char *ENCHANTED_BOOK = "minecraft:enchanted_book";
}

ItemStack LootItems::toItemStack(ServerNetworkHandler &owner, const LootDrop &drop) {
    const bool enchantedBook = drop.mIdentifier == BOOK && !drop.mEnchantments.empty();
    const std::string qualified = drop.mIdentifier.find(':') == std::string::npos ? "minecraft:" + drop.mIdentifier
                                                                                   : drop.mIdentifier;
    const std::string resolved = LegacyItemMapper::getInstance().resolve(qualified, drop.mData);
    const bool remapped = resolved != qualified;
    ItemStack stack = owner.createItemStack(enchantedBook ? ENCHANTED_BOOK : resolved, drop.mCount);
    if (stack.isAir())
        return stack;

    if (!remapped && drop.mData != 0 && stack.mDamage == 0 && drop.mDurabilityFraction >= 1.0f)
        stack.mDamage = drop.mData;

    if (drop.mDurabilityFraction < 1.0f) {
        const ItemData *data = ItemDataTable::find(drop.mIdentifier);
        if (data != nullptr && data->mMaxDurability > 0)
            stack.mDamage = (int32_t) std::lround((float) data->mMaxDurability * (1.0f - drop.mDurabilityFraction));
    }

    if (drop.mExtraData.isCompound() && !drop.mExtraData.getKeys().empty()) {
        if (!stack.mTag.isCompound())
            stack.mTag = Tag::ofCompound();
        for (const std::string &key: drop.mExtraData.getKeys())
            stack.mTag.put(key, *drop.mExtraData.get(key));
    }

    if (!drop.mEnchantments.empty())
        ItemEnchantments::write(stack, drop.mEnchantments);

    return stack;
}

void LootItems::fillContainer(ServerNetworkHandler &owner, Container &container, const std::vector<LootDrop> &drops,
                              std::mt19937 &random) {
    std::vector<int> freeSlots;
    for (int slot = 0; slot < container.getContainerSize(); ++slot) {
        if (container.getContainerItem(slot).isAir())
            freeSlots.push_back(slot);
    }

    for (const LootDrop &drop: drops) {
        if (freeSlots.empty())
            return;

        ItemStack stack = toItemStack(owner, drop);
        if (stack.isAir())
            continue;

        const size_t index = std::uniform_int_distribution<size_t>(0, freeSlots.size() - 1)(random);
        container.setContainerItem(freeSlots[index], std::move(stack));
        freeSlots.erase(freeSlots.begin() + (std::ptrdiff_t) index);
    }
}
