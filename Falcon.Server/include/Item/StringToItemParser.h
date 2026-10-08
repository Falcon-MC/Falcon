#pragma once

#include "Item/Item.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class StringToItemParser {
public:
    static StringToItemParser &getInstance();

    void registerItem(const std::string &alias, const std::function<Item()> &factory);

    void registerBlock(const std::string &alias, const std::function<Block()> &factory);

    bool parse(const std::string &input, Item &out) const;

    std::vector<std::string> getKnownAliases() const;

    /**
     * Forgets the aliases behavior packs added, so the next world only parses its own content.
     */
    void reset();

private:
    StringToItemParser();

    static std::string _normalize(const std::string &input);

    void _registerDefaults();

    std::unordered_map<std::string, std::function<Item()>> mFactories;
};
