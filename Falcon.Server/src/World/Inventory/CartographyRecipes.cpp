#include "World/Inventory/CartographyRecipes.h"

#include "World/Inventory/AnvilRepair.h"
#include "World/Item/EmptyMapItem.h"
#include "World/Item/MapItem.h"
#include "World/Level/Map/MapItemSavedData.h"

namespace {
    const char *const PAPER = "minecraft:paper";
    const char *const GLASS_PANE = "minecraft:glass_pane";
    const char *const COMPASS = "minecraft:compass";

    bool is(const ItemStack &item, const char *identifier) {
        return !item.isAir() && item.mDefinition->getIdentifier() == identifier;
    }

    void ensureTag(ItemStack &item) {
        if (!item.mTag.isCompound())
            item.mTag = Tag::ofCompound();
    }

    // sub_142F63880 and sub_142F63D30
    bool extend(const ItemStack &input, const CartographyRecipes::Maps &maps, ItemStack &result) {
        if (input.mDamage > (int32_t) MapItemType::Locator)
            return false;

        const MapItemSavedData *data = maps.mGetMap(MapItem::getMapId(input));
        if (data == nullptr || data->isLocked() || data->getScale() >= MapItemSavedData::MAX_SCALE)
            return false;

        result = input;
        result.mCount = 1;
        ensureTag(result);
        const bool scaled = result.mTag.getByte(MapItem::TAG_IS_INIT, 0) != 0;
        result.mTag.putLong(MapItem::TAG_MAP_ID, maps.mExpand(MapItem::getMapId(input), scaled));
        result.mTag.putByte(MapItem::TAG_IS_INIT, 1);
        return true;
    }

    // sub_142F644E0 and sub_142F64860
    bool lock(const ItemStack &input, const CartographyRecipes::Maps &maps, CartographyResult &result) {
        const int64_t mapId = MapItem::getMapId(input);
        const MapItemSavedData *data = maps.mGetMap(mapId);
        if (data == nullptr || data->isLocked())
            return false;

        const int64_t copyId = maps.mAllocateLockId(mapId);
        if (copyId == MapItemSavedData::INVALID_ID)
            return false;

        result.mResult = input;
        result.mResult.mCount = 1;
        ensureTag(result.mResult);
        result.mResult.mTag.putLong(MapItem::TAG_MAP_ID, copyId);
        result.mResult.mTag.putByte(MapItem::TAG_IS_INIT, 1);
        result.mResult.mDamage = (int32_t) MapItemType::Locked;
        result.mLock.mOriginalId = mapId;
        result.mLock.mCopyId = copyId;
        return true;
    }

    // sub_142F656B0: a compass makes a locator map out of a plain one
    bool upgrade(const ItemStack &input, ItemStack &result) {
        if (input.mDamage == (int32_t) MapItemType::Locator)
            return false;

        if (MapItem::isFilledMap(input)) {
            if (MapItem::displaysPlayers(input))
                return false;

            result = input;
            result.mCount = 1;
            ensureTag(result);
            result.mTag.putByte(MapItem::TAG_DISPLAY_PLAYERS, 1);
            return true;
        }

        result = input;
        result.mCount = 1;
        result.mDamage = (int32_t) MapItemType::Locator;
        return true;
    }

    void rename(ItemStack &item, const std::string &name) {
        ensureTag(item);
        Tag *display = item.mTag.get("display");
        if (name.empty()) {
            if (display != nullptr && display->isCompound()) {
                display->remove("Name");
                if (display->isEmpty())
                    item.mTag.remove("display");
            }
            return;
        }

        if (display == nullptr || !display->isCompound()) {
            item.mTag.put("display", Tag::ofCompound());
            display = item.mTag.get("display");
        }
        display->putString("Name", name);
    }
}

const char *const CartographyRecipes::CLONING_UUID = "442d85ed-8272-4543-a6f1-418f90ded05d";
const char *const CartographyRecipes::EXTENDING_UUID = "8b36268c-1829-483c-a0f1-993b7156a8f2";
const char *const CartographyRecipes::LOCKING_UUID = "602234e4-cac1-4353-8bb7-b1ebff70024b";
const char *const CartographyRecipes::UPGRADING_UUID = "98c84b38-1085-46bd-b1ce-dd38c159e6cc";

CartographyResult CartographyRecipes::createResult(const ItemStack &input, const ItemStack &additional,
                                                   const std::string *name, const Maps &maps) {
    CartographyResult result;
    const bool filledMap = MapItem::isFilledMap(input);
    if (!filledMap && !is(input, EmptyMapItem::IDENTIFIER))
        return result;

    bool crafted = false;
    if (!additional.isAir()) {
        if (filledMap && is(additional, PAPER))
            crafted = extend(input, maps, result.mResult);
        else if (filledMap && is(additional, EmptyMapItem::IDENTIFIER)) {
            // sub_142F63130: one copy per slot holding empty maps, on top of the original
            result.mResult = input;
            result.mResult.mCount = 2;
            crafted = true;
        } else if (filledMap && is(additional, GLASS_PANE))
            crafted = lock(input, maps, result);
        else if (is(additional, COMPASS))
            crafted = upgrade(input, result.mResult);

        if (!crafted) {
            result.mResult = ItemStack::air();
            return result;
        }
        result.mInputCost = 1;
        result.mAdditionalCost = 1;
    }

    const bool renaming = name != nullptr
                          && (*name
                              != (AnvilRepair::hasCustomName(input) ? input.mTag.get("display")->getString("Name")
                                                                    : std::string()));
    if (!crafted) {
        if (!renaming)
            return result;

        result.mResult = input;
        result.mInputCost = input.mCount;
    }

    if (renaming)
        rename(result.mResult, *name);
    return result;
}

bool CartographyRecipes::isLocking(const ItemStack &input, const ItemStack &additional, int64_t originalId) {
    return originalId != MapItemSavedData::INVALID_ID && MapItem::isFilledMap(input)
           && MapItem::getMapId(input) == originalId && input.mDamage != (int32_t) MapItemType::Locked
           && is(additional, GLASS_PANE);
}
