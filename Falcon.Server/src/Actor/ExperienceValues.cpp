#include "Actor/ExperienceValues.h"

#include <random>
#include <unordered_map>

namespace {

    const int ORB_SPLIT_SIZES[] = {2477, 1237, 617, 307, 149, 73, 37, 17, 7, 3, 1};

    std::mt19937 &experienceRandom() {
        static std::mt19937 generator(0x9E3779B9u);
        return generator;
    }

    int randomRange(int minInclusive, int maxInclusive) {
        std::uniform_int_distribution<int> distribution(minInclusive, maxInclusive);
        return distribution(experienceRandom());
    }

    int getMaxOrbSize(int amount) {
        for (int split: ORB_SPLIT_SIZES) {
            if (amount >= split)
                return split;
        }

        return 1;
    }

    const std::unordered_map<std::string, int> &oreExperienceRanges() {
        static const std::unordered_map<std::string, int> ores = {
                {"minecraft:coal_ore", 0},
                {"minecraft:deepslate_coal_ore", 0},
                {"minecraft:diamond_ore", 1},
                {"minecraft:deepslate_diamond_ore", 1},
                {"minecraft:emerald_ore", 2},
                {"minecraft:deepslate_emerald_ore", 2},
                {"minecraft:lapis_ore", 3},
                {"minecraft:deepslate_lapis_ore", 3},
                {"minecraft:redstone_ore", 4},
                {"minecraft:lit_redstone_ore", 4},
                {"minecraft:deepslate_redstone_ore", 4},
                {"minecraft:lit_deepslate_redstone_ore", 4},
                {"minecraft:quartz_ore", 5}
        };

        return ores;
    }

}

namespace ExperienceValues {

    std::vector<int> splitIntoOrbSizes(int amount) {
        std::vector<int> result;

        while (amount > 0) {
            const int size = getMaxOrbSize(amount);
            result.push_back(size);
            amount -= size;
        }

        return result;
    }

    int getOreDropExperience(const std::string &blockName) {
        const auto it = oreExperienceRanges().find(blockName);
        if (it == oreExperienceRanges().end())
            return 0;

        switch (it->second) {
            case 0:
                return randomRange(0, 2);
            case 1:
                return randomRange(3, 7);
            case 2:
                return randomRange(3, 7);
            case 3:
                return randomRange(2, 5);
            case 4:
                return randomRange(1, 5);
            case 5:
                return randomRange(1, 5);
            default:
                return 0;
        }
    }

}
