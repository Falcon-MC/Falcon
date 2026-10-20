#include "Item/StringToItemParser.h"

#include "Block/BlockData.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Item/ItemData.h"
#include "Item/VanillaItems.h"

StringToItemParser::StringToItemParser() {
    _registerDefaults();
}

StringToItemParser &StringToItemParser::getInstance() {
    static StringToItemParser instance;
    return instance;
}

void StringToItemParser::reset() {
    mFactories.clear();
    _registerDefaults();
}

std::string StringToItemParser::_normalize(const std::string &input) {
    std::string out;
    out.reserve(input.size());

    for (char c: input) {
        if (c >= 'A' && c <= 'Z') {
            out.push_back((char) (c - 'A' + 'a'));
            continue;
        }

        if (c == ' ' || c == '-') {
            out.push_back('_');
            continue;
        }

        out.push_back(c);
    }

    const std::string prefix = "minecraft:";
    if (out.size() > prefix.size() && out.compare(0, prefix.size(), prefix) == 0)
        return out.substr(prefix.size());

    return out;
}

void StringToItemParser::registerItem(const std::string &alias, const std::function<Item()> &factory) {
    mFactories[_normalize(alias)] = factory;
}

void StringToItemParser::registerBlock(const std::string &alias, const std::function<Block()> &factory) {
    mFactories[_normalize(alias)] = [factory]() {
        return Item(factory());
    };
}

void StringToItemParser::_registerDefaults() {
    for (size_t index = 0; index < BlockDataTable::getCount(); ++index) {
        const BlockData &data = BlockDataTable::getEntries()[index];
        registerBlock(data.mIdentifier, [&data]() {
            return VanillaBlocks::fromData(data);
        });
    }

    registerBlock("air", VanillaBlocks::AIR);
    registerBlock("grass", VanillaBlocks::GRASS);

    for (size_t i = 0; i < ItemDataTable::getCount(); ++i) {
        const ItemData &data = ItemDataTable::at(i);
        registerItem(data.mIdentifier, [&data]() {
            return Item(data);
        });
    }
}

bool StringToItemParser::parse(const std::string &input, Item &out) const {
    auto it = mFactories.find(_normalize(input));
    if (it == mFactories.end())
        return false;

    out = it->second();
    return true;
}

std::vector<std::string> StringToItemParser::getKnownAliases() const {
    std::vector<std::string> aliases;
    aliases.reserve(mFactories.size());

    for (const auto &entry: mFactories)
        aliases.push_back(entry.first);

    return aliases;
}
