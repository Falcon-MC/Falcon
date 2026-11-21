#include "World/Level/Storage/Loot/LootTableRegistry.h"

#include "Core/Json/Json.h"
#include "EntityLootTablesJson.h"
#include "LootTablesJson.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
    std::string readFile(const std::filesystem::path &path) {
        std::ifstream file(path, std::ios::binary);
        std::stringstream contents;
        contents << file.rdbuf();
        return contents.str();
    }
}

LootTableRegistry &LootTableRegistry::getInstance() {
    static LootTableRegistry instance;
    return instance;
}

LootTableRegistry::LootTableRegistry() {
    mDefinitions = json::parse(FalconLootData::kLootTablesJson);
    if (mDefinitions == nullptr || !mDefinitions->isObject())
        return;

    for (const auto &entry: mDefinitions->mObject) {
        if (entry.second->isObject())
            mTables[_normalize(entry.first)] = std::make_unique<LootTable>(*entry.second);
    }

    const std::unique_ptr<json::Value> entities = json::parse(FalconLootData::kEntityLootTablesJson);
    if (entities == nullptr || !entities->isObject())
        return;

    for (const auto &entry: entities->mObject)
        mEntityTables[entry.first] = _normalize(entry.second->string());
}

void LootTableRegistry::addPack(const std::string &packRoot) {
    namespace fs = std::filesystem;
    const fs::path root = fs::path(packRoot) / "loot_tables";
    std::error_code error;
    if (!fs::is_directory(root, error))
        return;
    for (fs::recursive_directory_iterator it(root, error), end; it != end && !error; it.increment(error)) {
        if (!it->is_regular_file(error) || it->path().extension() != ".json")
            continue;
        const std::string path = fs::relative(it->path(), root, error).generic_string();
        if (error || mPackTables.count(path) != 0)
            continue;
        std::unique_ptr<json::Value> definition = json::parse(readFile(it->path()));
        if (definition == nullptr || !definition->isObject())
            continue;
        mPackTables[path] = std::make_unique<LootTable>(*definition);
        mPackDefinitions[path] = std::move(definition);
    }
}

const LootTable *LootTableRegistry::get(const std::string &path) const {
    const std::string normalized = _normalize(path);
    const auto pack = mPackTables.find(normalized);
    if (pack != mPackTables.end())
        return pack->second.get();
    const auto table = mTables.find(normalized);
    return table == mTables.end() ? nullptr : table->second.get();
}

const json::Value *LootTableRegistry::getDefinition(const std::string &path) const {
    const std::string normalized = _normalize(path);
    const auto pack = mPackDefinitions.find(normalized);
    if (pack != mPackDefinitions.end())
        return pack->second.get();
    if (mDefinitions == nullptr || !mDefinitions->isObject())
        return nullptr;
    for (const auto &entry: mDefinitions->mObject) {
        if (_normalize(entry.first) == normalized)
            return entry.second.get();
    }
    return nullptr;
}

std::string LootTableRegistry::getEntityTablePath(const std::string &identifier) const {
    const auto path = mEntityTables.find(identifier);
    return path == mEntityTables.end() ? std::string() : path->second;
}

const LootTable *LootTableRegistry::getForEntity(const std::string &identifier) const {
    const auto path = mEntityTables.find(identifier);
    return path == mEntityTables.end() ? nullptr : get(path->second);
}

std::string LootTableRegistry::_normalize(const std::string &path) {
    static const std::string PREFIX = "loot_tables/";
    return path.compare(0, PREFIX.size(), PREFIX) == 0 ? path.substr(PREFIX.size()) : path;
}
