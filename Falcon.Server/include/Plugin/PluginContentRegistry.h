#pragma once

#include "Block/Block.h"
#include "Item/Item.h"

#include <falcon/falcon_api.h>

#include <memory>
#include <string>
#include <unordered_map>

struct LoadedPlugin;

class PluginContentRegistry {
public:
    struct ItemEntry {
        LoadedPlugin *mPlugin = nullptr;
        FalconItemUseHandler mOnUse = nullptr;
        FalconItemUseOnBlockHandler mOnUseOnBlock = nullptr;
        void *mUserData = nullptr;
    };

    struct BlockEntry {
        LoadedPlugin *mPlugin = nullptr;
        FalconBlockInteractHandler mOnInteract = nullptr;
        FalconBlockBreakHandler mOnBreak = nullptr;
        void *mUserData = nullptr;
        std::string mDrop;
    };

    struct EntityEntry {
        LoadedPlugin *mPlugin = nullptr;
        float mWidth = 0.6f;
        float mHeight = 1.8f;
        float mMaxHealth = 20.0f;
        FalconEntityTickHandler mOnTick = nullptr;
        FalconEntityInteractHandler mOnInteract = nullptr;
        void *mUserData = nullptr;
    };

    static PluginContentRegistry &getInstance();

    void addItem(const Item &base, const ItemEntry &entry);

    void addBlock(const Block &base, const BlockEntry &entry);

    void addEntity(const std::string &identifier, const EntityEntry &entry);

    const ItemEntry *findItem(const std::string &identifier) const;

    const BlockEntry *findBlock(const std::string &identifier) const;

    const EntityEntry *findEntity(const std::string &identifier) const;

    const Item *getItemType(const std::string &identifier) const;

    const Block *getBlockType(const std::string &identifier) const;

private:
    std::unordered_map<std::string, ItemEntry> mItems;
    std::unordered_map<std::string, BlockEntry> mBlocks;
    std::unordered_map<std::string, EntityEntry> mEntities;
    std::unordered_map<std::string, std::unique_ptr<Item>> mItemTypes;
    std::unordered_map<std::string, std::unique_ptr<Block>> mBlockTypes;
};
