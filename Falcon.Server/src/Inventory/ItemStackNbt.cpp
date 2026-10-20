#include "Inventory/ItemStackNbt.h"

#include "Block/BlockState.h"
#include "Core/BlockState/BlockStateData.h"
#include "Core/BlockState/BlockStateUpgradeSchema.h"
#include "Core/Debug/BedrockLog.h"
#include "Item/Loot/LegacyItemMapper.h"
#include "Level/BlockStateUpgrades.h"

namespace {
    const char *TAG_SLOT = "Slot";
    const char *TAG_NAME = "Name";
    const char *TAG_COUNT = "Count";
    const char *TAG_DAMAGE = "Damage";
    const char *TAG_WAS_PICKED_UP = "WasPickedUp";
    const char *TAG_USER_DATA = "tag";
    const char *TAG_BLOCK_NAME = "BlockName";
    const char *TAG_BLOCK = "Block";
    const char *TAG_CAN_PLACE = "CanPlaceOn";
    const char *TAG_CAN_BREAK = "CanDestroy";

    const char *AIR_IDENTIFIER = "minecraft:air";

    Tag stringList(const std::vector<std::string> &values) {
        Tag list = Tag::ofList(Tag::Type::String);
        for (const std::string &value: values) {
            list.addToList(Tag::ofString(value));
        }
        return list;
    }

    /**
     * Saved data can name an item the game has since renamed or split by meta. The current item is
     * looked up first, and the saved one is kept when the upgrade leads nowhere the registry knows.
     */
    std::shared_ptr<ItemDefinition> upgradeItem(const std::string &name, int32_t &damage,
                                                const PacketCodecContext &context) {
        std::string upgraded = name;
        int32_t upgradedDamage = damage;
        if (LegacyItemMapper::getInstance().upgrade(upgraded, upgradedDamage)) {
            std::shared_ptr<ItemDefinition> definition = context.getItemDefinitions().getDefinition(upgraded);
            if (definition != nullptr) {
                damage = upgradedDamage;
                return definition;
            }
        }

        return context.getItemDefinitions().getDefinition(name);
    }

    BlockStateData upgradeBlock(const Tag &block) {
        const BlockStateData stored = BlockStateData::fromNbt(block);
        try {
            return BlockStateUpgrades::upgrade(stored);
        } catch (const BlockStateUpgradeException &exception) {
            LOG_WARN(LogAreaID::Server, "Could not upgrade block %s on a saved item: %s", stored.getName().c_str(),
                     exception.what());
            return stored;
        }
    }

    void readStringList(const Tag &data, const char *key, std::vector<std::string> &target) {
        const Tag *list = data.get(key);
        if (list == nullptr || list->getType() != Tag::Type::List) {
            return;
        }

        for (const Tag &entry: list->getList()) {
            if (entry.getType() == Tag::Type::String) {
                target.push_back(entry.asString());
            }
        }
    }
}

Tag ItemStackNbt::write(const ItemStack &item) {
    Tag data = Tag::ofCompound();

    if (item.isAir() || item.mCount <= 0) {
        data.putString(TAG_NAME, std::string());
        data.putByte(TAG_COUNT, 0);
        data.putShort(TAG_DAMAGE, 0);
        data.putByte(TAG_WAS_PICKED_UP, 0);
        return data;
    }

    data.putString(TAG_NAME, item.mDefinition->getIdentifier());
    data.putByte(TAG_COUNT, (int8_t) item.mCount);
    data.putShort(TAG_DAMAGE, (int16_t) item.mDamage);
    data.putByte(TAG_WAS_PICKED_UP, 0);

    if (item.mTag.getType() == Tag::Type::Compound && !item.mTag.isEmpty()) {
        data.put(TAG_USER_DATA, item.mTag);
    }

    if (item.mBlockDefinition != nullptr) {
        data.put(TAG_BLOCK, BlockState(item.mBlockDefinition->getIdentifier(),
                                       item.mBlockDefinition->getState()).toNbt());
    }

    if (!item.mCanPlace.empty()) {
        data.put(TAG_CAN_PLACE, stringList(item.mCanPlace));
    }

    if (!item.mCanBreak.empty()) {
        data.put(TAG_CAN_BREAK, stringList(item.mCanBreak));
    }

    return data;
}

Tag ItemStackNbt::write(const ItemStack &item, int slot) {
    Tag data = write(item);
    data.putByte(TAG_SLOT, (int8_t) slot);
    return data;
}

int ItemStackNbt::readSlot(const Tag &data) {
    if (data.getType() != Tag::Type::Compound) {
        return -1;
    }
    return data.getByte(TAG_SLOT, -1);
}

ItemStack ItemStackNbt::read(const Tag &data, const PacketCodecContext &context) {
    if (data.getType() != Tag::Type::Compound) {
        return ItemStack::air();
    }

    const std::string name = data.getString(TAG_NAME);
    if (name.empty() || name == AIR_IDENTIFIER) {
        return ItemStack::air();
    }

    const int count = data.getByte(TAG_COUNT, 0);
    if (count <= 0) {
        return ItemStack::air();
    }

    int32_t damage = data.getShort(TAG_DAMAGE, 0);
    std::shared_ptr<ItemDefinition> definition = upgradeItem(name, damage, context);
    if (definition == nullptr) {
        LOG_ERROR(LogAreaID::Server, "Skipping unknown item %s while loading saved data", name.c_str());
        return ItemStack::air();
    }

    ItemStack item;
    item.mDefinition = definition;
    item.mCount = count;
    item.mDamage = damage;

    const Tag *userData = data.get(TAG_USER_DATA);
    if (userData != nullptr && userData->getType() == Tag::Type::Compound) {
        item.mTag = *userData;
    }

    const Tag *block = data.get(TAG_BLOCK);
    const std::string blockName = data.getString(TAG_BLOCK_NAME);
    if (block != nullptr && block->isCompound() && !block->getString("name", std::string()).empty()) {
        const BlockStateData state = upgradeBlock(*block);
        item.mBlockDefinition = context.getBlockDefinitions().getDefinition(
                BlockStateHasher::hash(state.getName(), state.getStates()));

        if (item.mBlockDefinition == nullptr)
            item.mBlockDefinition = context.getBlockDefinitions().getDefinition(state.getName());
    } else if (!blockName.empty()) {
        const std::string currentBlockName = BlockStateUpgrades::currentName(blockName);
        item.mBlockDefinition = context.getBlockDefinitions().getDefinition(currentBlockName);

        if (item.mBlockDefinition == nullptr)
            LOG_ERROR(LogAreaID::Server, "Unknown block %s on saved item %s", blockName.c_str(), name.c_str());
    } else {
        item.mBlockDefinition = context.getBlockDefinitions().getDefinition(definition->getIdentifier());
    }

    readStringList(data, TAG_CAN_PLACE, item.mCanPlace);
    readStringList(data, TAG_CAN_BREAK, item.mCanBreak);

    return item;
}
