#include "Item/Loot/LootTableRegistry.h"

#include "Core/Json/Json.h"
#include "EntityLootTablesJson.h"
#include "LootTablesJson.h"

LootTableRegistry &LootTableRegistry::getInstance() {
    static LootTableRegistry instance;
    return instance;
}

LootTableRegistry::LootTableRegistry() {
    const std::unique_ptr<json::Value> root = json::parse(FalconLootData::kLootTablesJson);
    if (root == nullptr || !root->isObject())
        return;

    for (const auto &entry: root->mObject) {
        if (entry.second->isObject())
            mTables[_normalize(entry.first)] = std::make_unique<LootTable>(*entry.second);
    }

    const std::unique_ptr<json::Value> entities = json::parse(FalconLootData::kEntityLootTablesJson);
    if (entities == nullptr || !entities->isObject())
        return;

    for (const auto &entry: entities->mObject)
        mEntityTables[entry.first] = _normalize(entry.second->string());
}

const LootTable *LootTableRegistry::get(const std::string &path) const {
    const auto table = mTables.find(_normalize(path));
    return table == mTables.end() ? nullptr : table->second.get();
}

const LootTable *LootTableRegistry::getForEntity(const std::string &identifier) const {
    const auto path = mEntityTables.find(identifier);
    return path == mEntityTables.end() ? nullptr : get(path->second);
}

std::string LootTableRegistry::_normalize(const std::string &path) {
    static const std::string PREFIX = "loot_tables/";
    return path.compare(0, PREFIX.size(), PREFIX) == 0 ? path.substr(PREFIX.size()) : path;
}
