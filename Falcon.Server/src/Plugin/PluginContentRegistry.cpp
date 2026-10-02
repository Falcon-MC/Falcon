#include "Plugin/PluginContentRegistry.h"

#include "Actor/ActorClassRegistry.h"
#include "Block/BlockClassRegistry.h"
#include "Item/ItemClassRegistry.h"
#include "Plugin/PluginActor.h"

PluginContentRegistry &PluginContentRegistry::getInstance() {
    static PluginContentRegistry instance;
    return instance;
}

void PluginContentRegistry::addItem(const Item &base, const ItemEntry &entry) {
    const std::string &identifier = base.getIdentifier();
    mItems[identifier] = entry;
    mItemTypes[identifier] = ItemClassRegistry::create(base);
}

void PluginContentRegistry::addBlock(const Block &base, const BlockEntry &entry) {
    const std::string &identifier = base.getIdentifier();
    mBlocks[identifier] = entry;
    mBlockTypes[identifier] = BlockClassRegistry::create(base);
}

void PluginContentRegistry::addEntity(const std::string &identifier, const EntityEntry &entry) {
    mEntities[identifier] = entry;
    const ActorClassRegistry::Registration registration(
            identifier.c_str(), [](uint64_t runtimeId, const std::string &actorIdentifier)
                    -> std::unique_ptr<ServerActor> {
                return std::make_unique<PluginActor>(runtimeId, actorIdentifier);
            });
    (void) registration;
}

const PluginContentRegistry::ItemEntry *PluginContentRegistry::findItem(const std::string &identifier) const {
    const auto it = mItems.find(identifier);
    return it == mItems.end() ? nullptr : &it->second;
}

const PluginContentRegistry::BlockEntry *PluginContentRegistry::findBlock(const std::string &identifier) const {
    const auto it = mBlocks.find(identifier);
    return it == mBlocks.end() ? nullptr : &it->second;
}

const PluginContentRegistry::EntityEntry *PluginContentRegistry::findEntity(const std::string &identifier) const {
    const auto it = mEntities.find(identifier);
    return it == mEntities.end() ? nullptr : &it->second;
}

const Item *PluginContentRegistry::getItemType(const std::string &identifier) const {
    const auto it = mItemTypes.find(identifier);
    return it == mItemTypes.end() ? nullptr : it->second.get();
}

const Block *PluginContentRegistry::getBlockType(const std::string &identifier) const {
    const auto it = mBlockTypes.find(identifier);
    return it == mBlockTypes.end() ? nullptr : it->second.get();
}
