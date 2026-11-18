#include "World/Item/DyeableItems.h"

#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Item/CraftingRecipeTable.h"

bool DyeableItems::takesColor(const std::string &identifier) {
    if (identifier == "minecraft:wolf_armor")
        return true;
    if (const CustomItemDefinition *custom = CustomContentRegistry::getInstance().findItem(identifier))
        return custom->mHasDyeable;
    for (const std::string &tag: CraftingRecipeTable::getItemTags(identifier)) {
        if (tag == "minecraft:leather_tier" || tag == "minecraft:horse_armor")
            return true;
    }
    return false;
}
