#pragma once

#include "Loot/LootTable.h"

#include <memory>
#include <string>
#include <unordered_map>

class LootTableRegistry {
public:
    static LootTableRegistry &getInstance();

    const LootTable *get(const std::string &path) const;

private:
    LootTableRegistry();

    static std::string _normalize(const std::string &path);

    std::unordered_map<std::string, std::unique_ptr<LootTable>> mTables;
};
