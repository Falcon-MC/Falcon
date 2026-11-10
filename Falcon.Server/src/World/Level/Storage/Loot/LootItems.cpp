#include "World/Level/Storage/Loot/LootItems.h"

#include "World/Inventory/Container.h"
#include "World/Item/ItemData.h"
#include "World/Level/Storage/Loot/LegacyItemMapper.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>
#include <cstddef>
#include <string>
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"

#include <iterator>
#include <utility>

namespace {
    template <typename T>
    void shuffle(std::vector<T> &values, std::mt19937 &random) {
        for (size_t i = 1; i < values.size(); ++i) {
            const size_t other = random() % (uint32_t) (i + 1);
            if (other != i)
                std::swap(values[i], values[other]);
        }
    }
}

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

std::vector<LootItems::PlacedDrop> LootItems::distribute(std::vector<LootDrop> drops, std::vector<int32_t> freeSlots,
                                                         std::mt19937 &random) {
    shuffle(freeSlots, random);

    std::vector<LootDrop> items;
    std::vector<LootDrop> splittable;
    for (LootDrop &drop: drops) {
        if (drop.mCount == 1)
            items.push_back(std::move(drop));
        else if (drop.mCount > 1)
            splittable.push_back(std::move(drop));
    }

    int32_t remaining = (int32_t) freeSlots.size() - (int32_t) items.size();
    while (remaining > 0 && !splittable.empty()) {
        const int32_t index = GameRandom::nextInt(random, (int32_t) splittable.size());
        LootDrop first = std::move(splittable[(size_t) index]);
        splittable.erase(splittable.begin() + index);

        const int32_t half = first.mCount / 2;
        const int32_t taken = half != 0 ? GameRandom::nextInt(random, half) + 1 : 1;
        LootDrop second = first;
        first.mCount -= taken;
        second.mCount = taken;

        // the two halves read the same coin the opposite way round
        if (first.mCount < 2 || !FeatureRandom::nextBoolean(random))
            items.push_back(std::move(first));
        else
            splittable.push_back(std::move(first));

        if (second.mCount < 2 || FeatureRandom::nextBoolean(random))
            items.push_back(std::move(second));
        else
            splittable.push_back(std::move(second));

        --remaining;
    }

    items.insert(items.begin(), std::make_move_iterator(splittable.begin()), std::make_move_iterator(splittable.end()));
    shuffle(items, random);

    std::vector<PlacedDrop> placed;
    for (LootDrop &item: items) {
        if (freeSlots.empty())
            break;

        placed.push_back({freeSlots.back(), std::move(item)});
        freeSlots.pop_back();
    }

    return placed;
}

void LootItems::fillContainer(ServerNetworkHandler &owner, Container &container, const std::vector<LootDrop> &drops,
                              std::mt19937 &random) {
    std::vector<int32_t> freeSlots;
    for (int slot = 0; slot < container.getContainerSize(); ++slot) {
        if (container.getContainerItem(slot).isAir())
            freeSlots.push_back(slot);
    }

    for (PlacedDrop &placed: distribute(drops, std::move(freeSlots), random)) {
        ItemStack stack = toItemStack(owner, placed.mDrop);
        if (!stack.isAir())
            container.setContainerItem(placed.mSlot, std::move(stack));
    }
}
