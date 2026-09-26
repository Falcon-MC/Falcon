#include "Item/EnchantmentHelper.h"

#include "Item/EnchantmentData.h"
#include "Item/ItemData.h"
#include "Item/ItemTypes.h"
#include "Level/Level.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace {

    const char *AIR = "minecraft:air";
    const char *BOOKSHELF = "minecraft:bookshelf";
    const char *BOOK = "minecraft:book";

    const int32_t MAX_BOOKSHELF_COUNT = 15;
    const int32_t FIRST_NET_ID = 100000;
    const int32_t NAME_MIN_LENGTH = 5;
    const int32_t NAME_MAX_LENGTH = 15;

    struct StoredOption {
        std::vector<EnchantmentInstance> mEnchantments;
        int32_t mEntry;
        int32_t mRequiredLevel;
    };

    std::unordered_map<int32_t, StoredOption> &recipeMap() {
        static std::unordered_map<int32_t, StoredOption> map;
        return map;
    }

    int32_t &nextNetId() {
        static int32_t value = FIRST_NET_ID;
        return value;
    }

    class SeededRandom {
    public:
        explicit SeededRandom(uint64_t seed) : mState(seed == 0 ? 0x9e3779b97f4a7c15ull : seed) {
        }

        uint32_t nextBits() {
            mState ^= mState << 13;
            mState ^= mState >> 7;
            mState ^= mState << 17;
            return (uint32_t) (mState >> 32);
        }

        int32_t rangeInclusive(int32_t min, int32_t max) {
            if (max <= min)
                return min;

            const uint32_t span = (uint32_t) (max - min + 1);
            return min + (int32_t) (nextBits() % span);
        }

        int32_t boundInclusive(int32_t max) {
            if (max <= 0)
                return 0;

            return (int32_t) (nextBits() % (uint32_t) (max + 1));
        }

        int32_t belowExclusive(int32_t bound) {
            if (bound <= 0)
                return 0;

            return (int32_t) (nextBits() % (uint32_t) bound);
        }

        float unit() {
            return (float) (nextBits() >> 8) / (float) (1u << 24);
        }

    private:
        uint64_t mState;
    };

    int32_t armorEnchantability(ArmorTier tier) {
        switch (tier) {
            case ArmorTier::Leather:
            case ArmorTier::Netherite:
                return 15;
            case ArmorTier::Chain:
                return 12;
            case ArmorTier::Iron:
                return 9;
            case ArmorTier::Gold:
                return 25;
            case ArmorTier::Diamond:
                return 10;
            default:
                return 0;
        }
    }

    int32_t toolEnchantability(ToolTier tier) {
        switch (tier) {
            case ToolTier::Wooden:
            case ToolTier::Netherite:
                return 15;
            case ToolTier::Stone:
                return 5;
            case ToolTier::Iron:
                return 14;
            case ToolTier::Gold:
                return 22;
            case ToolTier::Diamond:
                return 10;
            default:
                return 0;
        }
    }

    bool compatible(const EnchantmentData &left, const EnchantmentData &right) {
        if (left.mId == right.mId)
            return false;

        return (left.mIncompatibleGroups & right.mIncompatibleGroups) == 0u;
    }

    std::vector<EnchantmentData> availableEnchantments(int32_t power, const std::string &identifier,
                                                       bool allowTreasure) {
        std::vector<EnchantmentData> list;

        for (size_t index = 0; index < EnchantmentTable::getCount(); index++) {
            const EnchantmentData &enchantment = EnchantmentTable::at(index);
            if (!allowTreasure && EnchantmentHelper::isTreasure(enchantment.mId))
                continue;

            if (!ItemEnchantments::canApply(identifier, enchantment))
                continue;

            for (int32_t level = enchantment.mMaxLevel; level > 0; level--) {
                if (power >= EnchantmentHelper::getMinAbility(enchantment.mId, level)
                    && power <= EnchantmentHelper::getMaxAbility(enchantment.mId, level)) {
                    EnchantmentData chosen = enchantment;
                    chosen.mMaxLevel = level;
                    list.push_back(chosen);
                    break;
                }
            }
        }

        return list;
    }

    const EnchantmentData *weightedPick(SeededRandom &random, const std::vector<EnchantmentData> &enchantments) {
        if (enchantments.empty())
            return nullptr;

        int32_t totalWeight = 0;
        for (const EnchantmentData &enchantment: enchantments)
            totalWeight += EnchantmentTable::getRarityWeight(enchantment.mRarity);

        if (totalWeight <= 0)
            return nullptr;

        int32_t roll = random.belowExclusive(totalWeight);
        for (const EnchantmentData &enchantment: enchantments) {
            roll -= EnchantmentTable::getRarityWeight(enchantment.mRarity);
            if (roll < 0)
                return &enchantment;
        }

        return &enchantments.back();
    }

    std::string randomName(SeededRandom &random) {
        std::string name;
        const int32_t length = random.rangeInclusive(NAME_MIN_LENGTH, NAME_MAX_LENGTH);
        for (int32_t i = 0; i < length; i++)
            name.push_back((char) ('a' + random.belowExclusive('z' - 'a' + 1)));

        return name;
    }

    std::vector<EnchantmentInstance> rollEnchantments(SeededRandom &random, const std::string &identifier,
                                                      int32_t requiredLevel, bool allowTreasure) {
        int32_t power = requiredLevel;
        const int32_t enchantability = EnchantmentHelper::getEnchantability(identifier);
        const int32_t quarter = enchantability >> 2;
        power += random.boundInclusive(quarter) + random.boundInclusive(quarter) + 1;

        const double bonus = 1.0 + ((double) random.unit() + (double) random.unit() - 1.0) * 0.15;
        power = (int32_t) std::lround((double) power * bonus);
        if (power < 1)
            power = 1;

        std::vector<EnchantmentData> pool = availableEnchantments(power, identifier, allowTreasure);
        std::vector<EnchantmentInstance> result;

        if (!pool.empty()) {
            const EnchantmentData *last = weightedPick(random, pool);
            if (last != nullptr)
                result.push_back({last->mId, last->mMaxLevel});

            while (last != nullptr && random.rangeInclusive(1, 50) <= power) {
                const int32_t lastId = last->mId;
                std::vector<EnchantmentData> filtered;
                for (const EnchantmentData &candidate: pool) {
                    if (candidate.mId != lastId && compatible(candidate, *last))
                        filtered.push_back(candidate);
                }

                pool.swap(filtered);
                if (pool.empty())
                    break;

                last = weightedPick(random, pool);
                if (last == nullptr)
                    break;

                result.push_back({last->mId, last->mMaxLevel});
                power /= 2;
            }
        }

        if (identifier == BOOK && result.size() > 1)
            result.erase(result.begin() + random.belowExclusive((int32_t) result.size()));

        return result;
    }

}

int32_t EnchantmentHelper::getEnchantability(const std::string &identifier) {
    if (identifier == BOOK || identifier == "minecraft:bow" || identifier == "minecraft:crossbow"
        || identifier == "minecraft:fishing_rod")
        return 1;

    if (identifier == "minecraft:turtle_helmet")
        return 9;

    const ItemData *data = ItemDataTable::find(identifier);
    if (data == nullptr)
        return 0;

    if (data->mArmorSlot != ArmorSlot::None)
        return armorEnchantability((ArmorTier) data->mArmorTier);

    if (data->mToolType != ToolType::None)
        return toolEnchantability((ToolTier) data->mToolTier);

    return 0;
}

bool EnchantmentHelper::isTreasure(int32_t enchantmentId) {
    static const std::unordered_set<int32_t> treasure = {
            EnchantmentIds::MENDING, EnchantmentIds::FROST_WALKER, EnchantmentIds::BINDING,
            EnchantmentIds::VANISHING, EnchantmentIds::SOUL_SPEED, EnchantmentIds::SWIFT_SNEAK,
            EnchantmentIds::WIND_BURST
    };

    return treasure.count(enchantmentId) != 0;
}

int32_t EnchantmentHelper::getMinAbility(int32_t enchantmentId, int32_t level) {
    switch (enchantmentId) {
        case EnchantmentIds::PROTECTION:
            return 1 + (level - 1) * 11;
        case EnchantmentIds::FIRE_PROTECTION:
            return 10 + (level - 1) * 8;
        case EnchantmentIds::FEATHER_FALLING:
            return 5 + (level - 1) * 6;
        case EnchantmentIds::BLAST_PROTECTION:
            return 5 + (level - 1) * 8;
        case EnchantmentIds::PROJECTILE_PROTECTION:
            return 3 + (level - 1) * 6;
        case EnchantmentIds::THORNS:
            return 10 + (level - 1) * 20;
        case EnchantmentIds::RESPIRATION:
            return 10 * level;
        case EnchantmentIds::DEPTH_STRIDER:
            return level * 10;
        case EnchantmentIds::AQUA_AFFINITY:
            return 1;
        case EnchantmentIds::SHARPNESS:
            return 1 + (level - 1) * 11;
        case EnchantmentIds::SMITE:
        case EnchantmentIds::BANE_OF_ARTHROPODS:
            return 5 + (level - 1) * 8;
        case EnchantmentIds::KNOCKBACK:
            return 5 + (level - 1) * 20;
        case EnchantmentIds::FIRE_ASPECT:
            return 10 + (level - 1) * 20;
        case EnchantmentIds::LOOTING:
        case EnchantmentIds::FORTUNE:
        case EnchantmentIds::LUCK_OF_THE_SEA:
            return 15 + (level - 1) * 9;
        case EnchantmentIds::EFFICIENCY:
        case EnchantmentIds::POWER:
            return 1 + (level - 1) * 10;
        case EnchantmentIds::SILK_TOUCH:
            return 15;
        case EnchantmentIds::UNBREAKING:
            return 5 + (level - 1) * 8;
        case EnchantmentIds::PUNCH:
            return 12 + (level - 1) * 20;
        case EnchantmentIds::FLAME:
        case EnchantmentIds::INFINITY_ENCHANTMENT:
            return 20;
        case EnchantmentIds::LURE:
            return level * 9 + 6;
        case EnchantmentIds::IMPALING:
            return 8 * level - 7;
        case EnchantmentIds::RIPTIDE:
            return 7 * level + 10;
        case EnchantmentIds::LOYALTY:
            return 7 * level + 5;
        case EnchantmentIds::CHANNELING:
            return 25;
        case EnchantmentIds::MULTISHOT:
            return 20;
        case EnchantmentIds::PIERCING:
            return 1 + 10 * (level - 1);
        case EnchantmentIds::QUICK_CHARGE:
            return 12 + 20 * (level - 1);
        case EnchantmentIds::DENSITY:
        case EnchantmentIds::BREACH:
            return 10 + (level - 1) * 20;
        default:
            return 1 + level * 10;
    }
}

int32_t EnchantmentHelper::getMaxAbility(int32_t enchantmentId, int32_t level) {
    const int32_t minAbility = getMinAbility(enchantmentId, level);

    switch (enchantmentId) {
        case EnchantmentIds::PROTECTION:
            return minAbility + 11;
        case EnchantmentIds::FIRE_PROTECTION:
        case EnchantmentIds::BLAST_PROTECTION:
            return minAbility + 8;
        case EnchantmentIds::FEATHER_FALLING:
        case EnchantmentIds::PROJECTILE_PROTECTION:
            return minAbility + 6;
        case EnchantmentIds::SHARPNESS:
        case EnchantmentIds::SMITE:
        case EnchantmentIds::BANE_OF_ARTHROPODS:
        case EnchantmentIds::IMPALING:
            return minAbility + 20;
        case EnchantmentIds::RESPIRATION:
            return minAbility + 30;
        case EnchantmentIds::DEPTH_STRIDER:
            return minAbility + 15;
        case EnchantmentIds::AQUA_AFFINITY:
            return minAbility + 40;
        case EnchantmentIds::POWER:
            return minAbility + 15;
        case EnchantmentIds::PUNCH:
            return minAbility + 25;
        case EnchantmentIds::FLAME:
        case EnchantmentIds::INFINITY_ENCHANTMENT:
        case EnchantmentIds::MULTISHOT:
        case EnchantmentIds::CHANNELING:
            return 50;
        case EnchantmentIds::LOOTING:
        case EnchantmentIds::FORTUNE:
        case EnchantmentIds::LUCK_OF_THE_SEA:
        case EnchantmentIds::LURE:
            return minAbility + 45 + level;
        case EnchantmentIds::THORNS:
        case EnchantmentIds::KNOCKBACK:
        case EnchantmentIds::FIRE_ASPECT:
        case EnchantmentIds::EFFICIENCY:
        case EnchantmentIds::SILK_TOUCH:
        case EnchantmentIds::UNBREAKING:
        case EnchantmentIds::DENSITY:
        case EnchantmentIds::BREACH:
            return minAbility + 50;
        default:
            return minAbility + 5;
    }
}

int32_t EnchantmentHelper::countBookshelves(Level &level, const Vector3i &tablePosition) {
    int32_t count = 0;

    for (int32_t x = -2; x <= 2; x++) {
        for (int32_t z = -2; z <= 2; z++) {
            if (std::abs(x) != 2 && std::abs(z) != 2)
                continue;

            const int32_t gapX = std::max(std::min(x, 1), -1);
            const int32_t gapZ = std::max(std::min(z, 1), -1);

            bool blocked = false;
            for (int32_t y = 0; y <= 1; y++) {
                const BlockState gap = level.getBlockState(tablePosition.x + gapX, tablePosition.y + y,
                                                           tablePosition.z + gapZ);
                if (gap.mName != AIR) {
                    blocked = true;
                    break;
                }
            }

            if (blocked)
                continue;

            for (int32_t y = 0; y <= 1; y++) {
                const BlockState shelf = level.getBlockState(tablePosition.x + x, tablePosition.y + y,
                                                             tablePosition.z + z);
                if (shelf.mName == BOOKSHELF) {
                    count++;
                    if (count == MAX_BOOKSHELF_COUNT)
                        return count;
                }
            }
        }
    }

    return count;
}

std::vector<EnchantOptionData> EnchantmentHelper::getEnchantOptions(Level &level, const Vector3i &tablePosition,
                                                                    const ItemStack &input, int32_t seed) {
    std::vector<EnchantOptionData> options;

    if (input.isAir() || !ItemEnchantments::read(input).empty())
        return options;

    const std::string identifier = input.mDefinition == nullptr ? std::string()
                                                                : input.mDefinition->getIdentifier();
    if (identifier.empty())
        return options;

    SeededRandom random((uint64_t) (uint32_t) seed);

    const int32_t bookshelves = countBookshelves(level, tablePosition);
    const int32_t base = random.rangeInclusive(1, 8) + (bookshelves >> 1) + random.rangeInclusive(0, bookshelves);

    const int32_t levels[3] = {
            std::max(1, (int32_t) std::floor((double) base / 3.0)),
            std::max(1, (int32_t) std::floor((double) base * 2.0 / 3.0 + 1.0)),
            std::max(1, std::max(base, bookshelves * 2))
    };

    for (int32_t entry = 0; entry < 3; entry++) {
        const int32_t requiredLevel = levels[entry];
        const std::vector<EnchantmentInstance> rolled = rollEnchantments(random, identifier, requiredLevel, false);

        EnchantOptionData option;
        option.mCost = requiredLevel;
        option.mPrimarySlot = 0;
        option.mEnchantName = randomName(random);
        option.mEnchantNetId = nextNetId()++;

        for (const EnchantmentInstance &instance: rolled) {
            EnchantEntry wire;
            wire.mType = instance.mId;
            wire.mLevel = instance.mLevel;
            option.mEnchants0.push_back(wire);
        }

        StoredOption stored;
        stored.mEnchantments = rolled;
        stored.mEntry = entry;
        stored.mRequiredLevel = requiredLevel;
        recipeMap()[option.mEnchantNetId] = std::move(stored);

        options.push_back(std::move(option));
    }

    return options;
}

std::vector<EnchantmentInstance> EnchantmentHelper::enchantWithLevels(const std::string &identifier, int32_t levels,
                                                                      bool allowTreasure, uint64_t seed) {
    SeededRandom random(seed);
    return rollEnchantments(random, identifier, std::max(1, levels), allowTreasure);
}

std::vector<EnchantmentInstance> EnchantmentHelper::enchantRandomly(const std::string &identifier, bool allowTreasure,
                                                                    uint64_t seed) {
    std::vector<const EnchantmentData *> candidates;
    for (size_t index = 0; index < EnchantmentTable::getCount(); index++) {
        const EnchantmentData &enchantment = EnchantmentTable::at(index);
        if (!allowTreasure && isTreasure(enchantment.mId))
            continue;

        if (identifier == BOOK || ItemEnchantments::canApply(identifier, enchantment))
            candidates.push_back(&enchantment);
    }

    if (candidates.empty())
        return {};

    SeededRandom random(seed);
    const EnchantmentData *chosen = candidates[(size_t) random.belowExclusive((int32_t) candidates.size())];
    return {{chosen->mId, random.rangeInclusive(1, std::max(1, chosen->mMaxLevel))}};
}

bool EnchantmentHelper::peekOptionCost(int32_t enchantNetId, int32_t &outConsumeCost) {
    const std::unordered_map<int32_t, StoredOption>::iterator found = recipeMap().find(enchantNetId);
    if (found == recipeMap().end())
        return false;

    outConsumeCost = found->second.mEntry + 1;
    return true;
}

bool EnchantmentHelper::takeOption(int32_t enchantNetId, std::vector<EnchantmentInstance> &outEnchantments,
                                   int32_t &outConsumeCost, int32_t &outRequiredLevel) {
    const std::unordered_map<int32_t, StoredOption>::iterator found = recipeMap().find(enchantNetId);
    if (found == recipeMap().end())
        return false;

    outEnchantments = found->second.mEnchantments;
    outConsumeCost = found->second.mEntry + 1;
    outRequiredLevel = found->second.mRequiredLevel;
    recipeMap().erase(found);
    return true;
}
