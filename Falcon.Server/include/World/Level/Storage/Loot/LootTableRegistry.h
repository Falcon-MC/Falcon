#pragma once

#include "World/Level/Storage/Loot/LootTable.h"

#include <memory>
#include <string>
#include <unordered_map>

class LootTableRegistry {
public:
    static LootTableRegistry &getInstance();

    const LootTable *get(const std::string &path) const;

    const LootTable *getForEntity(const std::string &identifier) const;

    // the table as its JSON file holds it, nullptr when unknown
    const json::Value *getDefinition(const std::string &path) const;

    // the table path an entity type drops from, empty when it has none
    std::string getEntityTablePath(const std::string &identifier) const;

    // the pack's loot_tables folder; a pack table replaces the vanilla one, the first pack keeps a path
    void addPack(const std::string &packRoot);

private:
    LootTableRegistry();

    static std::string _normalize(const std::string &path);

    std::unordered_map<std::string, std::unique_ptr<LootTable>> mTables;
    std::unordered_map<std::string, std::unique_ptr<LootTable>> mPackTables;
    std::unordered_map<std::string, std::unique_ptr<json::Value>> mPackDefinitions;
    std::unique_ptr<json::Value> mDefinitions;
    std::unordered_map<std::string, std::string> mEntityTables;
};
