#include "World/Actor/Trade/TradeTableRegistry.h"

#include "Core/Json/Json.h"
#include "TradeTablesJson.h"

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

TradeTableRegistry &TradeTableRegistry::getInstance() {
    static TradeTableRegistry instance;
    return instance;
}

TradeTableRegistry::TradeTableRegistry() {
    const std::unique_ptr<json::Value> definitions = json::parse(FalconTradeData::kTradeTablesJson);
    if (definitions == nullptr || !definitions->isObject())
        return;

    for (const auto &entry: definitions->mObject) {
        if (!entry.second->isObject())
            continue;
        std::unique_ptr<TradeTable> table = TradeTable::parse(*entry.second);
        if (table != nullptr)
            mTables[_normalize(entry.first)] = std::move(table);
    }
}

void TradeTableRegistry::addPack(const std::string &packRoot) {
    namespace fs = std::filesystem;
    const fs::path root = fs::path(packRoot) / "trading";
    std::error_code error;
    if (!fs::is_directory(root, error))
        return;
    for (fs::recursive_directory_iterator it(root, error), end; it != end && !error; it.increment(error)) {
        if (!it->is_regular_file(error) || it->path().extension() != ".json")
            continue;
        const std::string path = fs::relative(it->path(), root, error).generic_string();
        if (error || mPackTables.count(path) != 0)
            continue;
        const std::unique_ptr<json::Value> definition = json::parse(readFile(it->path()));
        if (definition == nullptr || !definition->isObject())
            continue;
        std::unique_ptr<TradeTable> table = TradeTable::parse(*definition);
        if (table != nullptr)
            mPackTables[path] = std::move(table);
    }
}

const TradeTable *TradeTableRegistry::get(const std::string &path) const {
    const auto pack = mPackTables.find(_normalize(path));
    if (pack != mPackTables.end())
        return pack->second.get();
    const auto table = mTables.find(_normalize(path));
    return table == mTables.end() ? nullptr : table->second.get();
}

std::string TradeTableRegistry::_normalize(const std::string &path) {
    static const std::string PREFIX = "trading/";
    return path.compare(0, PREFIX.size(), PREFIX) == 0 ? path.substr(PREFIX.size()) : path;
}
