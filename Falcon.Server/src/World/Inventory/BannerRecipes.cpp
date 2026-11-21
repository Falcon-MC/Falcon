#include "World/Inventory/BannerRecipes.h"

#include "World/Item/BannerPatterns.h"

#include <string>

namespace {
    const char *const BANNER = "minecraft:banner";
    const char *const SHIELD = "minecraft:shield";
    const char *const TAG_BASE = "Base";
    const char *const TAG_TYPE = "Type";
    const char *const TAG_PATTERNS = "Patterns";
    const char *const TAG_PATTERN = "Pattern";
    const char *const TAG_COLOR = "Color";
    const int32_t OMINOUS_TYPE = 1;
    // a shield takes banners of up to six layers, what a loom can make
    const int32_t SHIELD_MAX_PATTERNS = 7;
    const int32_t DUPLICATE_COUNT = 2;

    int32_t duplicateNetworkId = 0;
    int32_t shieldNetworkId = 0;

    bool is(const ItemStack &item, const char *identifier) {
        return !item.isAir() && item.mDefinition != nullptr && item.mDefinition->getIdentifier() == identifier;
    }

    // the Base the banner saves, or its colour as the item's data
    int32_t baseOf(const ItemStack &banner) {
        if (banner.mTag.isCompound() && banner.mTag.contains(TAG_BASE))
            return banner.mTag.getInt(TAG_BASE, 0);
        return banner.mDamage & 0xF;
    }

    ItemStack single(const ItemStack &item, int32_t count) {
        ItemStack copy = item;
        copy.mCount = count;
        copy.mUsingNetId = false;
        copy.mNetId = 0;
        return copy;
    }

    void merge(Tag &into, const Tag &from) {
        if (!from.isCompound())
            return;
        const std::vector<std::string> &keys = from.getKeys();
        const std::vector<Tag> &values = from.getValues();
        for (size_t index = 0; index < keys.size() && index < values.size(); ++index)
            into.put(keys[index], values[index]);
    }
}

const char *const BannerRecipes::DUPLICATE_UUID = "b5c5d105-75a2-4076-af2b-923ea2bf4bf0";
const char *const BannerRecipes::SHIELD_UUID = "00000000-0000-0000-0000-0000000000c8";

void BannerRecipes::setNetworkIds(int32_t duplicate, int32_t shield) {
    duplicateNetworkId = duplicate;
    shieldNetworkId = shield;
}

BannerRecipes::Kind BannerRecipes::kindOf(int32_t networkId) {
    if (networkId > 0 && networkId == duplicateNetworkId)
        return Kind::Duplicate;
    if (networkId > 0 && networkId == shieldNetworkId)
        return Kind::Shield;
    return Kind::None;
}

bool BannerRecipes::duplicate(const std::vector<ItemStack> &grid, Craft &craft) {
    int32_t patterned = -1;
    int32_t blank = -1;
    for (size_t slot = 0; slot < grid.size(); ++slot) {
        const ItemStack &item = grid[slot];
        if (item.isAir())
            continue;
        // an ominous banner can't be copied, nor be a copy
        if (!BannerPatterns::isBanner(item))
            return false;

        const bool hasPatterns = BannerPatterns::patternCount(item) > 0;
        const int32_t base = baseOf(item);
        if (hasPatterns) {
            if (patterned >= 0 || (blank >= 0 && baseOf(grid[(size_t) blank]) != base))
                return false;
            patterned = (int32_t) slot;
        } else {
            if (blank >= 0 || (patterned >= 0 && baseOf(grid[(size_t) patterned]) != base))
                return false;
            blank = (int32_t) slot;
        }
    }
    if (patterned < 0 || blank < 0)
        return false;

    craft.mResult = single(grid[(size_t) patterned], DUPLICATE_COUNT);
    craft.mRequiredBySlot.assign(grid.size(), 0);
    craft.mRequiredBySlot[(size_t) patterned] = 1;
    craft.mRequiredBySlot[(size_t) blank] = 1;
    return true;
}

// any other item in the grid is left out of the craft, as the game ignores it
bool BannerRecipes::decorateShield(const std::vector<ItemStack> &grid, Craft &craft) {
    int32_t shield = -1;
    int32_t banner = -1;
    int32_t shields = 0;
    int32_t banners = 0;
    for (size_t slot = 0; slot < grid.size(); ++slot) {
        const ItemStack &item = grid[slot];
        if (is(item, SHIELD)) {
            if (item.mTag.isCompound() && item.mTag.contains(TAG_BASE))
                return false;
            ++shields;
            shield = (int32_t) slot;
        } else if (is(item, BANNER)) {
            if (BannerPatterns::patternCount(item) >= SHIELD_MAX_PATTERNS)
                return false;
            ++banners;
            banner = (int32_t) slot;
        }
    }
    if (shields != 1 || banners != 1)
        return false;

    const ItemStack &bannerItem = grid[(size_t) banner];
    ItemStack result = single(grid[(size_t) shield], 1);
    Tag tag = Tag::ofCompound();
    merge(tag, result.mTag);
    if (bannerItem.mTag.isCompound()) {
        // the ominous banner's look is the client's own: the shield gets its illager layer spelled out
        if (bannerItem.mTag.getInt(TAG_TYPE, 0) == OMINOUS_TYPE) {
            Tag layer = Tag::ofCompound();
            layer.putString(TAG_PATTERN, "ill");
            layer.putInt(TAG_COLOR, 15);
            std::vector<Tag> layers;
            layers.push_back(std::move(layer));
            tag.put(TAG_PATTERNS, Tag::ofList(Tag::Type::Compound, std::move(layers)));
        } else {
            merge(tag, bannerItem.mTag);
        }
    }
    tag.putInt(TAG_BASE, bannerItem.mDamage);
    result.mTag = std::move(tag);

    craft.mResult = std::move(result);
    craft.mRequiredBySlot.assign(grid.size(), 0);
    craft.mRequiredBySlot[(size_t) shield] = 1;
    craft.mRequiredBySlot[(size_t) banner] = 1;
    return true;
}

bool BannerRecipes::craft(Kind kind, const std::vector<ItemStack> &grid, Craft &craft) {
    switch (kind) {
        case Kind::Duplicate:
            return duplicate(grid, craft);
        case Kind::Shield:
            return decorateShield(grid, craft);
        case Kind::None:
            break;
    }
    return false;
}
