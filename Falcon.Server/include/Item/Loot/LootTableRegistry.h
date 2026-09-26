#pragma once

#include "Item/Loot/LootTable.h"

#include <memory>
#include <string>
#include <unordered_map>

class LootTableRegistry {
public:
    static LootTableRegistry &getInstance();

    const LootTable *get(const std::string &path) const;

    const LootTable *getForEntity(const std::string &identifier) const;

private:
    LootTableRegistry();

    static std::string _normalize(const std::string &path);

    std::unordered_map<std::string, std::unique_ptr<LootTable>> mTables;
    std::unordered_map<std::string, std::string> mEntityTables;
};
