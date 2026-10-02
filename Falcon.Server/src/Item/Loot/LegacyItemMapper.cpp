#include "Item/Loot/LegacyItemMapper.h"

#include "Core/Json/Json.h"
#include "LegacyItemMapJson.h"

#include <cstdlib>
#include <memory>

const LegacyItemMapper &LegacyItemMapper::getInstance() {
    static const LegacyItemMapper instance;
    return instance;
}

LegacyItemMapper::LegacyItemMapper() {
    const std::unique_ptr<json::Value> root = json::parse(FalconLegacyItemData::kLegacyItemMapJson);
    if (root == nullptr || !root->isObject())
        return;

    const json::Value *simple = root->get("simple");
    if (simple != nullptr && simple->isObject()) {
        for (const auto &entry: simple->mObject)
            mSimple[entry.first] = entry.second->string();
    }

    const json::Value *complex = root->get("complex");
    if (complex != nullptr && complex->isObject()) {
        for (const auto &entry: complex->mObject) {
            if (!entry.second->isObject())
                continue;

            std::unordered_map<int32_t, std::string> &values = mComplex[entry.first];
            for (const auto &value: entry.second->mObject)
                values[(int32_t) std::strtol(value.first.c_str(), nullptr, 10)] = value.second->string();
        }
    }
}

void LegacyItemMapper::splitData(const std::string &identifierAndData, std::string &identifier, int32_t &data) {
    identifier = identifierAndData;
    data = 0;

    const size_t separator = identifierAndData.rfind(':');
    if (separator == std::string::npos || separator + 1 >= identifierAndData.size())
        return;

    const std::string suffix = identifierAndData.substr(separator + 1);
    if (suffix.find_first_not_of("0123456789") != std::string::npos)
        return;

    identifier = identifierAndData.substr(0, separator);
    data = (int32_t) std::strtol(suffix.c_str(), nullptr, 10);
}

std::string LegacyItemMapper::resolveWithData(const std::string &identifierAndData) const {
    std::string identifier;
    int32_t data = 0;
    splitData(identifierAndData, identifier, data);
    return resolve(identifier, data);
}

std::string LegacyItemMapper::resolve(const std::string &identifier, int32_t data) const {
    std::string name = identifier.find(':') == std::string::npos ? "minecraft:" + identifier : identifier;

    const auto complex = mComplex.find(name);
    if (complex != mComplex.end()) {
        const auto value = complex->second.find(data);
        if (value != complex->second.end())
            return value->second;
    }

    const auto simple = mSimple.find(name);
    if (simple != mSimple.end())
        return simple->second;

    return name;
}
