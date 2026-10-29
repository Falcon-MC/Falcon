#include "World/Item/ItemEnchantments.h"

#include "World/Item/ItemData.h"

const char *ItemEnchantments::TAG_ENCHANTMENTS = "ench";
const char *ItemEnchantments::TAG_ENCHANTMENT_ID = "id";
const char *ItemEnchantments::TAG_ENCHANTMENT_LEVEL = "lvl";

namespace {
    bool endsWith(const std::string &value, const std::string &suffix) {
        if (value.size() < suffix.size()) {
            return false;
        }
        return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    uint32_t targetsFromToolType(ToolType toolType) {
        switch (toolType) {
            case ToolType::Sword:
                return EnchantmentTargets::SWORD;
            case ToolType::Shovel:
                return EnchantmentTargets::SHOVEL;
            case ToolType::Pickaxe:
                return EnchantmentTargets::PICKAXE;
            case ToolType::Axe:
                return EnchantmentTargets::AXE;
            case ToolType::Shears:
                return EnchantmentTargets::SHEARS;
            case ToolType::Hoe:
                return EnchantmentTargets::HOE;
            default:
                return EnchantmentTargets::NONE;
        }
    }

    uint32_t targetsFromArmorSlot(ArmorSlot armorSlot) {
        switch (armorSlot) {
            case ArmorSlot::Head:
                return EnchantmentTargets::HELMET;
            case ArmorSlot::Chest:
                return EnchantmentTargets::CHESTPLATE;
            case ArmorSlot::Legs:
                return EnchantmentTargets::LEGGINGS;
            case ArmorSlot::Feet:
                return EnchantmentTargets::BOOTS;
            default:
                return EnchantmentTargets::NONE;
        }
    }
}

uint32_t ItemEnchantments::getItemTargets(const std::string &identifier) {
    if (identifier == "minecraft:trident") {
        return EnchantmentTargets::TRIDENT;
    }
    if (identifier == "minecraft:mace") {
        return EnchantmentTargets::MACE;
    }
    if (identifier == "minecraft:bow") {
        return EnchantmentTargets::BOW;
    }
    if (identifier == "minecraft:crossbow") {
        return EnchantmentTargets::CROSSBOW;
    }
    if (identifier == "minecraft:fishing_rod") {
        return EnchantmentTargets::FISHING_ROD;
    }
    if (identifier == "minecraft:carrot_on_a_stick" || identifier == "minecraft:warped_fungus_on_a_stick") {
        return EnchantmentTargets::CARROT_ON_STICK;
    }
    if (identifier == "minecraft:shield") {
        return EnchantmentTargets::SHIELD;
    }
    if (identifier == "minecraft:elytra") {
        return EnchantmentTargets::ELYTRA;
    }
    if (identifier == "minecraft:brush") {
        return EnchantmentTargets::BRUSH;
    }
    if (identifier == "minecraft:flint_and_steel") {
        return EnchantmentTargets::FLINT_AND_STEEL;
    }
    if (identifier == "minecraft:shears") {
        return EnchantmentTargets::SHEARS;
    }
    if (identifier == "minecraft:compass" || identifier == "minecraft:lodestone_compass" ||
        identifier == "minecraft:recovery_compass") {
        return EnchantmentTargets::COMPASS;
    }
    if (identifier == "minecraft:book" || identifier == "minecraft:enchanted_book") {
        return EnchantmentTargets::BOOK;
    }
    if (identifier == "minecraft:skull" || identifier == "minecraft:carved_pumpkin" || endsWith(identifier, "_head")) {
        return EnchantmentTargets::MASK;
    }
    if (endsWith(identifier, "_spear")) {
        return EnchantmentTargets::SPEAR;
    }

    const ItemData *data = ItemDataTable::find(identifier);
    if (data == nullptr) {
        return EnchantmentTargets::NONE;
    }

    uint32_t targets = targetsFromToolType(data->mToolType);
    targets |= targetsFromArmorSlot(data->mArmorSlot);

    return targets;
}

bool ItemEnchantments::canApply(const std::string &identifier, const EnchantmentData &enchantment) {
    const uint32_t targets = getItemTargets(identifier);
    if (targets == EnchantmentTargets::NONE) {
        return false;
    }

    return (targets & (enchantment.mPrimaryTargets | enchantment.mSecondaryTargets)) != 0;
}

std::vector<EnchantmentInstance> ItemEnchantments::read(const ItemStack &item) {
    std::vector<EnchantmentInstance> result;

    if (item.mTag.getType() != Tag::Type::Compound) {
        return result;
    }

    const Tag *list = item.mTag.get(TAG_ENCHANTMENTS);
    if (list == nullptr || list->getType() != Tag::Type::List) {
        return result;
    }

    for (const Tag &entry: list->getList()) {
        if (entry.getType() != Tag::Type::Compound) {
            continue;
        }

        EnchantmentInstance instance;
        instance.mId = entry.getShort(TAG_ENCHANTMENT_ID, -1);
        instance.mLevel = entry.getShort(TAG_ENCHANTMENT_LEVEL, 0);

        if (instance.mId < 0 || instance.mLevel <= 0) {
            continue;
        }

        result.push_back(instance);
    }

    return result;
}

void ItemEnchantments::write(ItemStack &item, const std::vector<EnchantmentInstance> &enchantments) {
    if (item.mTag.getType() != Tag::Type::Compound) {
        item.mTag = Tag::ofCompound();
    }

    if (enchantments.empty()) {
        item.mTag.remove(TAG_ENCHANTMENTS);
        return;
    }

    Tag list = Tag::ofList(Tag::Type::Compound);
    for (const EnchantmentInstance &instance: enchantments) {
        Tag entry = Tag::ofCompound();
        entry.putShort(TAG_ENCHANTMENT_ID, (int16_t) instance.mId);
        entry.putShort(TAG_ENCHANTMENT_LEVEL, (int16_t) instance.mLevel);
        list.addToList(entry);
    }

    item.mTag.put(TAG_ENCHANTMENTS, list);
}

int32_t ItemEnchantments::getLevel(const ItemStack &item, int32_t enchantmentId) {
    for (const EnchantmentInstance &instance: read(item)) {
        if (instance.mId == enchantmentId) {
            return instance.mLevel;
        }
    }

    return 0;
}

EnchantmentApplyResult ItemEnchantments::apply(ItemStack &item, const EnchantmentData &enchantment, int32_t level,
                                               const EnchantmentData **conflict) {
    if (conflict != nullptr) {
        *conflict = nullptr;
    }

    if (level < 1 || level > enchantment.mMaxLevel) {
        return EnchantmentApplyResult::InvalidLevel;
    }

    if (item.isAir()) {
        return EnchantmentApplyResult::IncompatibleItem;
    }

    if (!canApply(item.mDefinition->getIdentifier(), enchantment)) {
        return EnchantmentApplyResult::IncompatibleItem;
    }

    std::vector<EnchantmentInstance> enchantments = read(item);

    for (const EnchantmentInstance &instance: enchantments) {
        if (instance.mId == enchantment.mId) {
            continue;
        }

        const EnchantmentData *existing = EnchantmentTable::findById(instance.mId);
        if (existing == nullptr) {
            continue;
        }

        if ((existing->mIncompatibleGroups & enchantment.mIncompatibleGroups) != 0) {
            if (conflict != nullptr) {
                *conflict = existing;
            }
            return EnchantmentApplyResult::IncompatibleEnchantment;
        }
    }

    bool replaced = false;
    for (EnchantmentInstance &instance: enchantments) {
        if (instance.mId == enchantment.mId) {
            instance.mLevel = level;
            replaced = true;
            break;
        }
    }

    if (!replaced) {
        EnchantmentInstance instance;
        instance.mId = enchantment.mId;
        instance.mLevel = level;
        enchantments.push_back(instance);
    }

    write(item, enchantments);

    return EnchantmentApplyResult::Success;
}
