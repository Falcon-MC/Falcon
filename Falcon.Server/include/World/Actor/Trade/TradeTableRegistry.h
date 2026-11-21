#pragma once

#include "World/Actor/Trade/TradeTable.h"

#include <memory>
#include <string>
#include <unordered_map>

class TradeTableRegistry {
public:
    static TradeTableRegistry &getInstance();

    // a path as entity components name it (trading/economy_trades/armorer_trades.json); nullptr when unknown
    const TradeTable *get(const std::string &path) const;

    // the pack's trading folder; a pack table replaces the vanilla one, the first pack keeps a path
    void addPack(const std::string &packRoot);

private:
    TradeTableRegistry();

    static std::string _normalize(const std::string &path);

    std::unordered_map<std::string, std::unique_ptr<TradeTable>> mTables;
    std::unordered_map<std::string, std::unique_ptr<TradeTable>> mPackTables;
};
