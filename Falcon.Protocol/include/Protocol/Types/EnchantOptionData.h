#pragma once

#include <cstdint>
#include <string>
#include <vector>

class EnchantEntry {
public:
    int32_t mType = 0;
    int32_t mLevel = 0;
};

class EnchantOptionData {
public:
    int32_t mCost = 0;
    int32_t mPrimarySlot = 0;
    std::vector<EnchantEntry> mEnchants0;
    std::vector<EnchantEntry> mEnchants1;
    std::vector<EnchantEntry> mEnchants2;
    std::string mEnchantName;
    int32_t mEnchantNetId = 0;
};
