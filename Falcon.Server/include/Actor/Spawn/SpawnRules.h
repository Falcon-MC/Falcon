#pragma once

#include "Core/Json/Json.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct SpawnHerd {
    int32_t mMinSize = 1;
    int32_t mMaxSize = 1;
};

struct SpawnPermutation {
    int32_t mWeight = 0;
    std::string mIdentifier;
};

struct SpawnCondition {
    bool mSupported = true;
    bool mOnSurface = false;
    bool mUnderground = false;
    bool mUnderwater = false;
    bool mLava = false;
    bool mNoBubbles = false;
    int32_t mWeight = 0;
    std::vector<SpawnHerd> mHerds;
    std::vector<SpawnPermutation> mPermutations;
    bool mHasBrightness = false;
    int32_t mMinBrightness = 0;
    int32_t mMaxBrightness = 15;
    bool mAdjustForWeather = false;
    int32_t mMinDifficulty = 0;
    int32_t mMaxDifficulty = 3;
    bool mHasHeight = false;
    int32_t mMinHeight = 0;
    int32_t mMaxHeight = 0;
    bool mHasDistance = false;
    float mMinDistance = 0.0f;
    float mMaxDistance = 0.0f;
    int64_t mMinWorldAge = 0;
    int32_t mSurfaceDensity = -1;
    int32_t mUndergroundDensity = -1;
    std::vector<std::string> mOnBlocks;
    std::vector<std::string> mPreventedBlocks;
    std::shared_ptr<json::Value> mBiomeFilter;
};

struct SpawnRule {
    std::string mIdentifier;
    std::string mPopulation;
    std::vector<SpawnCondition> mConditions;
};

struct DespawnRule {
    bool mFromDistance = false;
    float mMinDistance = 32.0f;
    float mMaxDistance = 128.0f;
    bool mFromChance = true;
    int32_t mRandomChance = 800;
    bool mFromInactivity = true;
    int32_t mInactivitySeconds = 30;
};

class SpawnRules {
public:
    static void initialize();

    static const std::vector<SpawnRule> &getRules();

    static const std::string &getPopulation(const std::string &identifier);

    static const DespawnRule *getDespawnRule(const std::string &identifier);

private:
    static std::vector<SpawnRule> &_rules();

    static std::unordered_map<std::string, std::string> &_populations();

    static std::unordered_map<std::string, DespawnRule> &_despawnRules();
};
