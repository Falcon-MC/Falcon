#include "World/Actor/Trade/TradeTableRegistry.h"

#include "Core/Json/Json.h"
#include "TradeTablesJson.h"

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

const TradeTable *TradeTableRegistry::get(const std::string &path) const {
    const auto table = mTables.find(_normalize(path));
    return table == mTables.end() ? nullptr : table->second.get();
}

std::string TradeTableRegistry::_normalize(const std::string &path) {
    static const std::string PREFIX = "trading/";
    return path.compare(0, PREFIX.size(), PREFIX) == 0 ? path.substr(PREFIX.size()) : path;
}
