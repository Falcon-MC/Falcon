#include "Loot/LootTableRegistry.h"

#include "Core/Json/Json.h"
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
}

const LootTable *LootTableRegistry::get(const std::string &path) const {
    const auto table = mTables.find(_normalize(path));
    return table == mTables.end() ? nullptr : table->second.get();
}

std::string LootTableRegistry::_normalize(const std::string &path) {
    static const std::string PREFIX = "loot_tables/";
    return path.compare(0, PREFIX.size(), PREFIX) == 0 ? path.substr(PREFIX.size()) : path;
}
