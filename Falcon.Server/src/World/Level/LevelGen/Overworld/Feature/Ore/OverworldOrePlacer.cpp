#include "World/Level/LevelGen/Overworld/Feature/Ore/OverworldOrePlacer.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
    constexpr float PI = 3.1415927f;
    constexpr const char *UNDERGROUND_PASS = "underground_pass";

    struct ReplaceRule {
        Block (*mPlaces)();
        std::vector<std::string> mMayReplace;
    };

    // Random::nextInt(n) (sub_141A7E190)
    int32_t nextInt(std::mt19937 &random, int32_t bound) {
        return bound != 0 ? static_cast<int32_t>(random() % static_cast<uint32_t>(bound)) : 0;
    }

    // Random::nextFloat (sub_141A7E230)
    float nextFloat(std::mt19937 &random) {
        return static_cast<float>(static_cast<double>(random()) * 2.3283064365386963e-10);
    }

    // the game's sine table, filled with sinf (sub_14199D950)
    const std::vector<float> &sineTable() {
        static const std::vector<float> table = [] {
            std::vector<float> values(65536);
            for (int32_t i = 0; i < 65536; i++)
                values[(size_t) i] = std::sin(static_cast<float>(i) / 10430.378f);
            return values;
        }();
        return table;
    }

    float sineAt(float scaled) {
        return sineTable()[static_cast<uint16_t>(static_cast<int32_t>(scaled))];
    }

    // HashedString's hash (FNV-1, 64 bits), kept by each feature rule
    uint64_t hashedString(const std::string &value) {
        uint64_t hash = 0xCBF29CE484222325ULL;
        for (char character: value)
            hash = (hash * 0x100000001B3ULL) ^ static_cast<uint8_t>(character);
        return hash;
    }

    // a scatter bound written as a plain number; anything else is an expression this placer does not evaluate
    bool constant(const std::string &text, float &value) {
        if (text.empty())
            return false;

        char *end = nullptr;
        errno = 0;
        value = std::strtof(text.c_str(), &end);
        return errno == 0 && end != nullptr && *end == '\0';
    }

    struct Bounds {
        int32_t mDistribution;
        int32_t mMin;
        int32_t mMax;
    };

    bool bounds(const BiomeFeatureEntry::Coordinate &coordinate, Bounds &result) {
        float min = 0.0f;
        if (!constant(coordinate.mMin, min))
            return false;

        result.mDistribution = coordinate.mDistribution;
        result.mMin = static_cast<int32_t>(std::round(min));
        result.mMax = result.mMin;
        if (coordinate.mDistribution == 0)
            return true;

        float max = 0.0f;
        if (!constant(coordinate.mMax, max))
            return false;

        result.mMax = static_cast<int32_t>(std::round(max));
        // uniform and triangle are the only distributions the overworld ore rules use
        return coordinate.mDistribution == 1 || coordinate.mDistribution == 6;
    }

    // sub_143815010, sub_1437FD570
    int32_t sample(const Bounds &bounds, std::mt19937 &random) {
        if (bounds.mDistribution == 0 || bounds.mMax <= bounds.mMin)
            return bounds.mMin;

        if (bounds.mDistribution == 1)
            return nextInt(random, bounds.mMax - bounds.mMin) + bounds.mMin;

        const int32_t range = bounds.mMax - bounds.mMin;
        const int32_t rest = range - range / 2;
        const int32_t first = range >= -1 ? nextInt(random, range / 2 + 1) : 0;
        const int32_t second = rest >= 0 ? nextInt(random, rest + 1) : 0;
        return bounds.mMin + second + first;
    }

    // axis visiting order of each coordinate_eval_order (sub_1437FD990)
    const int32_t EVAL_ORDERS[6][3] = {
            {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
    };

    struct Sphere {
        float mX;
        float mY;
        float mZ;
        float mRadiusSquared;
        float mRadius;
    };

    bool contains(const std::vector<std::string> &names, const BlockState &state) {
        for (const std::string &name: names) {
            if (state.mName == name)
                return true;
        }
        return false;
    }

    // sub_14A2C9860
    bool exposedToAir(BlockManager &blocks, int32_t x, int32_t y, int32_t z) {
        return blocks.getBlockAt(x, y - 1, z).mName == "minecraft:air"
               || blocks.getBlockAt(x, y + 1, z).mName == "minecraft:air"
               || blocks.getBlockAt(x, y, z - 1).mName == "minecraft:air"
               || blocks.getBlockAt(x, y, z + 1).mName == "minecraft:air"
               || blocks.getBlockAt(x - 1, y, z).mName == "minecraft:air"
               || blocks.getBlockAt(x + 1, y, z).mName == "minecraft:air";
    }
}

// a minecraft:ore_feature of the behavior packs
struct OverworldOrePlacer::OreDefinition {
    const char *mIdentifier;
    int32_t mCount;
    float mDiscardChanceOnAirExposure;
    std::vector<ReplaceRule> mRules;
};

const OverworldOrePlacer::OreDefinition *OverworldOrePlacer::_find(const BiomeFeatureEntry &entry) {
    // BDS 1.26.61 behavior packs, features/*.json
    static const std::vector<OreDefinition> ORES = {
            {"minecraft:andesite_feature", 64, 0.0f, {{&VanillaBlocks::ANDESITE, {"minecraft:stone"}}}},
            {"minecraft:clay_ore_feature", 33, 0.0f, {{&VanillaBlocks::CLAY, {"minecraft:stone"}}}},
            {"minecraft:coal_ore_feature", 17, 0.5f, {{&VanillaBlocks::COAL_ORE, {"minecraft:stone"}},
                                                      {&VanillaBlocks::DEEPSLATE_COAL_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:coal_ore_upper_feature", 17, 0.0f, {{&VanillaBlocks::COAL_ORE, {"minecraft:stone"}},
                                                            {&VanillaBlocks::DEEPSLATE_COAL_ORE,
                                                             {"minecraft:deepslate"}}}},
            {"minecraft:copper_ore_feature", 10, 0.0f, {{&VanillaBlocks::COPPER_ORE, {"minecraft:stone"}},
                                                        {&VanillaBlocks::DEEPSLATE_COPPER_ORE,
                                                         {"minecraft:deepslate"}}}},
            {"minecraft:deepslate_feature", 33, 0.0f, {{&VanillaBlocks::DEEPSLATE, {"minecraft:stone"}}}},
            {"minecraft:diamond_ore_buried_feature", 8, 1.0f, {{&VanillaBlocks::DIAMOND_ORE, {"minecraft:stone"}},
                                                               {&VanillaBlocks::DEEPSLATE_DIAMOND_ORE,
                                                                {"minecraft:deepslate"}}}},
            {"minecraft:diamond_ore_feature", 4, 0.5f, {{&VanillaBlocks::DIAMOND_ORE, {"minecraft:stone"}},
                                                        {&VanillaBlocks::DEEPSLATE_DIAMOND_ORE,
                                                         {"minecraft:deepslate"}}}},
            {"minecraft:diamond_ore_large_feature", 12, 0.7f, {{&VanillaBlocks::DIAMOND_ORE, {"minecraft:stone"}},
                                                               {&VanillaBlocks::DEEPSLATE_DIAMOND_ORE,
                                                                {"minecraft:deepslate"}}}},
            // not in the packs, only in the game's definitions/features/dirt_feature.json
            {"minecraft:dirt_feature", 33, 0.0f, {{&VanillaBlocks::DIRT, {"minecraft:stone"}}}},
            {"minecraft:diorite_feature", 64, 0.0f, {{&VanillaBlocks::DIORITE, {"minecraft:stone"}}}},
            {"minecraft:dripstone_caves_copper_ore_feature", 20, 0.0f,
             {{&VanillaBlocks::COPPER_ORE, {"minecraft:stone"}},
              {&VanillaBlocks::DEEPSLATE_COPPER_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:emerald_ore_feature", 3, 0.0f, {{&VanillaBlocks::EMERALD_ORE, {"minecraft:stone"}},
                                                        {&VanillaBlocks::DEEPSLATE_EMERALD_ORE,
                                                         {"minecraft:deepslate"}}}},
            {"minecraft:gold_ore_extra_feature", 9, 0.0f, {{&VanillaBlocks::GOLD_ORE, {"minecraft:stone"}},
                                                           {&VanillaBlocks::DEEPSLATE_GOLD_ORE,
                                                            {"minecraft:deepslate"}}}},
            {"minecraft:gold_ore_feature", 9, 0.5f, {{&VanillaBlocks::GOLD_ORE, {"minecraft:stone"}},
                                                     {&VanillaBlocks::DEEPSLATE_GOLD_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:gold_ore_lower_feature", 9, 0.5f, {{&VanillaBlocks::GOLD_ORE, {"minecraft:stone"}},
                                                           {&VanillaBlocks::DEEPSLATE_GOLD_ORE,
                                                            {"minecraft:deepslate"}}}},
            {"minecraft:granite_feature", 64, 0.0f, {{&VanillaBlocks::GRANITE, {"minecraft:stone"}}}},
            {"minecraft:gravel_ore_feature", 33, 0.0f, {{&VanillaBlocks::GRAVEL,
                                                         {"minecraft:stone", "minecraft:deepslate"}}}},
            {"minecraft:iron_ore_feature", 10, 0.0f, {{&VanillaBlocks::IRON_ORE, {"minecraft:stone"}},
                                                      {&VanillaBlocks::DEEPSLATE_IRON_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:iron_ore_small_feature", 4, 0.0f, {{&VanillaBlocks::IRON_ORE, {"minecraft:stone"}},
                                                           {&VanillaBlocks::DEEPSLATE_IRON_ORE,
                                                            {"minecraft:deepslate"}}}},
            {"minecraft:lapis_ore_buried_feature", 7, 1.0f, {{&VanillaBlocks::LAPIS_ORE, {"minecraft:stone"}},
                                                             {&VanillaBlocks::DEEPSLATE_LAPIS_ORE,
                                                              {"minecraft:deepslate"}}}},
            {"minecraft:lapis_ore_feature", 7, 0.0f, {{&VanillaBlocks::LAPIS_ORE, {"minecraft:stone"}},
                                                      {&VanillaBlocks::DEEPSLATE_LAPIS_ORE, {"minecraft:deepslate"}}}},
            {"minecraft:redstone_ore_feature", 8, 0.0f, {{&VanillaBlocks::REDSTONE_ORE, {"minecraft:stone"}},
                                                         {&VanillaBlocks::DEEPSLATE_REDSTONE_ORE,
                                                          {"minecraft:deepslate"}}}},
            {"minecraft:silverfish_feature", 8, 0.0f, {{&VanillaBlocks::INFESTED_STONE, {"minecraft:stone"}},
                                                       {&VanillaBlocks::INFESTED_DEEPSLATE,
                                                        {"minecraft:deepslate"}}}},
            {"minecraft:tuff_feature", 64, 0.0f, {{&VanillaBlocks::TUFF, {"minecraft:stone", "minecraft:deepslate"}}}}
    };

    if (entry.mPass != UNDERGROUND_PASS)
        return nullptr;

    for (const OreDefinition &ore: ORES) {
        if (entry.mFeature == ore.mIdentifier)
            return &ore;
    }
    return nullptr;
}

bool OverworldOrePlacer::handles(const BiomeFeatureEntry &entry) {
    if (_find(entry) == nullptr)
        return false;

    float value = 0.0f;
    if (!constant(entry.mIterations, value))
        return false;
    if ((entry.mChanceNumerator <= 0 || entry.mChanceDenominator <= 0) && !constant(entry.mChancePercent, value))
        return false;
    if (entry.mEvalOrder < 0 || entry.mEvalOrder > 5)
        return false;

    for (const BiomeFeatureEntry::Coordinate &coordinate: entry.mCoordinates) {
        Bounds unused{};
        if (!bounds(coordinate, unused))
            return false;
    }
    return true;
}

void OverworldOrePlacer::apply(BlockManager &blocks, const LevelChunk &chunk, uint32_t levelSeed,
                               const std::vector<int32_t> &biomes) {
    // the decoration seed of sub_14CDD4880, from the level seed and the chunk position
    std::mt19937 random(levelSeed);
    const uint32_t multiplierX = (random() >> 1) | 1u;
    const uint32_t multiplierZ = (random() >> 1) | 1u;
    const uint32_t decorationSeed = (multiplierZ * static_cast<uint32_t>(chunk.getZ())
                                     + multiplierX * static_cast<uint32_t>(chunk.getX())) ^ levelSeed;

    // sub_14CDDF960, sub_14A22E470: every biome of the chunk, then its rules in order
    for (int32_t biome: biomes) {
        const std::vector<BiomeFeatureEntry> *rules = BiomeChunkGenDataRegistry::getConsolidatedFeatures(biome);
        if (rules == nullptr)
            continue;

        for (const BiomeFeatureEntry &entry: *rules) {
            if (!handles(entry))
                continue;

            _placeRule(blocks, chunk, entry, *_find(entry), decorationSeed, biomes.size() > 1 ? biome : -1);
        }
    }
}

void OverworldOrePlacer::_placeRule(BlockManager &blocks, const LevelChunk &chunk, const BiomeFeatureEntry &entry,
                                    const OreDefinition &ore, uint32_t decorationSeed, int32_t biomeFilter) {
    // sub_14A22D800: boost-style combine of the decoration seed with the rule's hash
    const uint32_t hash = static_cast<uint32_t>(hashedString(entry.mIdentifier));
    const uint32_t seed = decorationSeed ^ ((decorationSeed >> 2) + hash + (decorationSeed << 6) + 0x9E3779B9u);
    std::mt19937 scatter(seed);

    // sub_1437FC150
    int32_t iterations = 0;
    bool chosen = true;
    if (entry.mChanceNumerator > 0 && entry.mChanceDenominator > 0) {
        if (entry.mChanceNumerator != entry.mChanceDenominator
            && nextInt(scatter, entry.mChanceDenominator) >= entry.mChanceNumerator)
            chosen = false;
    } else {
        float percent = 0.0f;
        constant(entry.mChancePercent, percent);
        if (percent < 100.0f && (percent <= 0.0f || percent <= nextFloat(scatter) * 100.0f))
            chosen = false;
    }

    if (chosen) {
        float value = 0.0f;
        constant(entry.mIterations, value);
        const float rounded = std::round(value);
        if (rounded > 0.0f)
            iterations = static_cast<int32_t>(rounded);
    }

    if (iterations == 0)
        return;

    Bounds axes[3];
    for (size_t axis = 0; axis < 3; axis++)
        bounds(entry.mCoordinates[axis], axes[axis]);

    const int32_t origin[3] = {chunk.getX() << 4, 0, chunk.getZ() << 4};
    // the feature draws from a second twister on the same seed, shared by every iteration
    std::mt19937 placement(seed);
    while (iterations-- > 0) {
        int32_t position[3] = {0, 0, 0};
        for (int32_t axis: EVAL_ORDERS[entry.mEvalOrder])
            position[axis] = origin[axis] + sample(axes[axis], scatter);

        if (biomeFilter >= 0
            && static_cast<int32_t>(chunk.getBiomeAt(position[0] & 15, position[1], position[2] & 15)) != biomeFilter)
            continue;

        _placeOre(blocks, ore, position[0], position[1], position[2], placement);
    }
}

bool OverworldOrePlacer::_placeOre(BlockManager &blocks, const OreDefinition &ore, int32_t x, int32_t y, int32_t z,
                                   std::mt19937 &random) {
    // OreFeature::place (sub_14A2D6E50)
    const float size = static_cast<float>(ore.mCount);
    const float inverse = 1.0f / size;

    const float angle = (nextFloat(random) * PI) * 10430.378f;
    const float sine = sineAt(angle);
    const float cosine = sineAt(angle + 16384.0f);
    const float reach = size * 0.125f;
    const float alongX = reach * sine;
    const float alongZ = reach * cosine;
    const float originX = static_cast<float>(x);
    const float originZ = static_cast<float>(z);
    const float originY = static_cast<float>(y);

    const int32_t firstRoll = nextInt(random, 3);
    const float startX = (alongX + 8.0f) + originX;
    const float startZ = (alongZ + 8.0f) + originZ;
    const int32_t secondRoll = nextInt(random, 3);
    const float endX = (8.0f - alongX) + originX;
    const float endZ = (8.0f - alongZ) + originZ;
    const float endY = (static_cast<float>(secondRoll) + -2.0f) + originY;
    const float startY = (static_cast<float>(firstRoll) + -2.0f) + originY;

    const float deltaX = endX - startX;
    const float deltaY = endY - startY;
    const float deltaZ = endZ - startZ;
    const float length = std::sqrt(deltaZ * deltaZ + (deltaY * deltaY + deltaX * deltaX));
    const bool degenerate = length < 0.0001f;
    const float directionX = degenerate ? 0.0f : deltaX / length;
    const float directionZ = degenerate ? 0.0f : deltaZ / length;
    const float directionY = degenerate ? 0.0f : deltaY / length;
    const float horizontal = std::sqrt(directionZ * directionZ + directionX * directionX);
    const bool vertical = horizontal < 0.0001f;
    const float flatX = vertical ? 0.0f : directionX / horizontal;
    const float flatZ = vertical ? 0.0f : directionZ / horizontal;
    const float waveStep = PI * inverse;

    std::vector<Sphere> spheres((size_t) ore.mCount);
    for (int32_t i = 0; i < ore.mCount; i++) {
        const float step = static_cast<float>(i);
        Sphere &sphere = spheres[(size_t) i];
        sphere.mZ = (deltaZ * step) * inverse + startZ;
        sphere.mX = inverse * (deltaX * step) + startX;
        sphere.mY = inverse * (deltaY * step) + startY;
        const float roll = nextFloat(random);
        const float wave = sineAt((step * waveStep) * 10430.378f);
        const float radius = ((wave + 1.0f) * ((roll * size) * 0.0625f) + 1.0f) * 0.5f;
        sphere.mRadiusSquared = radius * radius;
        sphere.mRadius = std::sqrt(sphere.mRadiusSquared);
    }

    // spheres inside a later, larger one are dropped
    std::vector<const Sphere *> kept;
    float maxRadiusSquared = 0.0f;
    for (size_t i = 0; i < spheres.size(); i++) {
        const Sphere &sphere = spheres[i];
        bool inside = false;
        for (size_t j = i + 1; j < spheres.size() && !inside; j++) {
            const Sphere &other = spheres[j];
            if (!(sphere.mRadius <= other.mRadius))
                continue;

            const float dx = other.mX - sphere.mX;
            const float dy = other.mY - sphere.mY;
            const float dz = other.mZ - sphere.mZ;
            inside = other.mRadius > std::sqrt(dz * dz + (dy * dy + dx * dx)) + sphere.mRadius;
        }

        if (inside)
            continue;

        kept.push_back(&sphere);
        maxRadiusSquared = std::fmax(sphere.mRadiusSquared, maxRadiusSquared);
    }

    if (kept.empty())
        return false;

    int32_t minX = INT32_MAX;
    int32_t minY = INT32_MAX;
    int32_t minZ = INT32_MAX;
    int32_t maxX = INT32_MIN;
    int32_t maxY = INT32_MIN;
    int32_t maxZ = INT32_MIN;
    for (const Sphere *sphere: kept) {
        minX = std::min(minX, static_cast<int32_t>(std::floor(sphere->mX - sphere->mRadius)));
        minY = std::min(minY, static_cast<int32_t>(std::floor(sphere->mY - sphere->mRadius)));
        minZ = std::min(minZ, static_cast<int32_t>(std::floor(sphere->mZ - sphere->mRadius)));
        maxX = std::max(maxX, static_cast<int32_t>(std::floor(sphere->mRadius + sphere->mX)));
        maxY = std::max(maxY, static_cast<int32_t>(std::floor(sphere->mY + sphere->mRadius)));
        maxZ = std::max(maxZ, static_cast<int32_t>(std::floor(sphere->mZ + sphere->mRadius)));
    }

    std::vector<BlockState> states;
    for (const ReplaceRule &rule: ore.mRules)
        states.push_back(rule.mPlaces().toBlockState());

    bool placed = false;
    float centerX = static_cast<float>(minX) + 0.5f;
    for (int32_t blockX = minX; blockX <= maxX; blockX++, centerX += 1.0f) {
        const float flatAlongX = flatX * (centerX - startX);
        const float alongFromX = (centerX - startX) * directionX;
        float centerZ = static_cast<float>(minZ) + 0.5f;
        for (int32_t blockZ = minZ; blockZ <= maxZ; blockZ++, centerZ += 1.0f) {
            const float offsetZ = centerZ - startZ;
            const float projection = flatZ * offsetZ + flatAlongX;
            const float awayX = centerX - (projection * flatX + startX);
            const float awayZ = centerZ - (projection * flatZ + startZ);
            if (!(awayZ * awayZ + awayX * awayX <= maxRadiusSquared))
                continue;

            const float alongFromZ = offsetZ * directionZ;
            float centerY = static_cast<float>(minY) + 0.5f;
            for (int32_t blockY = minY; blockY <= maxY; blockY++, centerY += 1.0f) {
                const float along = ((centerY - startY) * directionY + alongFromX) + alongFromZ;
                const float lineX = centerX - (along * directionX + startX);
                const float lineZ = centerZ - (along * directionZ + startZ);
                const float lineY = centerY - (directionY * along + startY);
                if (lineZ * lineZ + (lineY * lineY + lineX * lineX) > maxRadiusSquared)
                    continue;

                bool inside = false;
                for (const Sphere *sphere: kept) {
                    const float dx = centerX - sphere->mX;
                    const float dz = centerZ - sphere->mZ;
                    const float dy = centerY - sphere->mY;
                    if (sphere->mRadiusSquared > dz * dz + (dy * dy + dx * dx)) {
                        inside = true;
                        break;
                    }
                }

                if (!inside)
                    continue;

                const BlockState &current = blocks.getBlockAt(blockX, blockY, blockZ);
                for (size_t index = 0; index < ore.mRules.size(); index++) {
                    if (!contains(ore.mRules[index].mMayReplace, current))
                        continue;

                    const BlockState &state = states[index];
                    if (current == state)
                        continue;

                    const float discard = ore.mDiscardChanceOnAirExposure;
                    if (discard > 0.0f && !(discard < 1.0f && nextFloat(random) >= discard)
                        && exposedToAir(blocks, blockX, blockY, blockZ))
                        continue;

                    blocks.setBlockStateAt(blockX, blockY, blockZ, state);
                    placed = true;
                    break;
                }
            }
        }
    }
    return placed;
}
