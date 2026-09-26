#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

class LegacyItemMapper {
public:
    static const LegacyItemMapper &getInstance();

    std::string resolve(const std::string &identifier, int32_t data) const;

private:
    LegacyItemMapper();

    std::unordered_map<std::string, std::string> mSimple;
    std::unordered_map<std::string, std::unordered_map<int32_t, std::string>> mComplex;
};
