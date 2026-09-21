#pragma once

#include "Core/Math/Vector3i.h"
#include "Item/ItemEnchantments.h"
#include "Protocol/Types/EnchantOptionData.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <vector>

class Level;

namespace EnchantmentHelper {

    const int32_t RECIPE_ID_BASE = 100000;

    int32_t getEnchantability(const std::string &identifier);

    bool isTreasure(int32_t enchantmentId);

    int32_t getMinAbility(int32_t enchantmentId, int32_t level);

    int32_t getMaxAbility(int32_t enchantmentId, int32_t level);

    int32_t countBookshelves(Level &level, const Vector3i &tablePosition);

    std::vector<EnchantOptionData> getEnchantOptions(Level &level, const Vector3i &tablePosition,
                                                     const ItemStack &input, int32_t seed);

    std::vector<EnchantmentInstance> enchantWithLevels(const std::string &identifier, int32_t levels,
                                                       bool allowTreasure, uint64_t seed);

    std::vector<EnchantmentInstance> enchantRandomly(const std::string &identifier, bool allowTreasure,
                                                     uint64_t seed);

    bool takeOption(int32_t enchantNetId, std::vector<EnchantmentInstance> &outEnchantments, int32_t &outConsumeCost,
                    int32_t &outRequiredLevel);

}
