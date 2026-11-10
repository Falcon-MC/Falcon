#include "World/Level/LevelGen/Overworld/Biome/OverworldBiomeParameters.h"

#include "World/Level/LevelGen/Biome/BiomeIds.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>

namespace {

    using Parameter = OverworldBiomeParameters::Parameter;
    using Space = OverworldBiomeParameters::Space;
    using Node = OverworldBiomeParameters::Node;

    constexpr int32_t NONE = -1;
    constexpr size_t CHILDREN_PER_NODE = 6;

    // biome tables as the game stores them from xmmword_14F49ABC8, with the tables it picks for new worlds
    const int32_t OCEANS[2][5] = {{BiomeIds::DEEP_FROZEN_OCEAN, BiomeIds::DEEP_COLD_OCEAN, BiomeIds::DEEP_OCEAN,
                                   BiomeIds::DEEP_LUKEWARM_OCEAN, BiomeIds::WARM_OCEAN},
                                  {BiomeIds::FROZEN_OCEAN, BiomeIds::COLD_OCEAN, BiomeIds::OCEAN,
                                   BiomeIds::LUKEWARM_OCEAN, BiomeIds::WARM_OCEAN}};

    const int32_t MIDDLE_BIOMES[5][5] = {
            {BiomeIds::ICE_PLAINS, BiomeIds::ICE_PLAINS, BiomeIds::ICE_PLAINS, BiomeIds::COLD_TAIGA, BiomeIds::TAIGA},
            {BiomeIds::PLAINS, BiomeIds::PLAINS, BiomeIds::FOREST, BiomeIds::TAIGA, BiomeIds::REDWOOD_TAIGA_MUTATED},
            {BiomeIds::FLOWER_FOREST, BiomeIds::PLAINS, BiomeIds::FOREST, BiomeIds::BIRCH_FOREST,
             BiomeIds::ROOFED_FOREST},
            {BiomeIds::SAVANNA, BiomeIds::SAVANNA, BiomeIds::FOREST, BiomeIds::JUNGLE, BiomeIds::JUNGLE},
            {BiomeIds::DESERT, BiomeIds::DESERT, BiomeIds::DESERT, BiomeIds::DESERT, BiomeIds::DESERT}};

    const int32_t MIDDLE_BIOMES_VARIANT[5][5] = {
            {BiomeIds::ICE_PLAINS_SPIKES, NONE, BiomeIds::COLD_TAIGA, NONE, NONE},
            {BiomeIds::DAPPLED_FOREST, NONE, NONE, NONE, BiomeIds::MEGA_TAIGA},
            {BiomeIds::SUNFLOWER_PLAINS, NONE, NONE, BiomeIds::BIRCH_FOREST_MUTATED, NONE},
            {NONE, NONE, BiomeIds::PLAINS, BiomeIds::JUNGLE_EDGE, BiomeIds::BAMBOO_JUNGLE},
            {NONE, NONE, NONE, NONE, NONE}};

    // the pale garden table, chosen for worlds newer than the gate in sub_14CDB0CB0
    const int32_t PLATEAU_BIOMES[5][5] = {
            {BiomeIds::ICE_PLAINS, BiomeIds::ICE_PLAINS, BiomeIds::ICE_PLAINS, BiomeIds::COLD_TAIGA,
             BiomeIds::COLD_TAIGA},
            {BiomeIds::MEADOW, BiomeIds::MEADOW, BiomeIds::FOREST, BiomeIds::TAIGA, BiomeIds::REDWOOD_TAIGA_MUTATED},
            {BiomeIds::MEADOW, BiomeIds::MEADOW, BiomeIds::MEADOW, BiomeIds::MEADOW, BiomeIds::PALE_GARDEN},
            {BiomeIds::SAVANNA_PLATEAU, BiomeIds::SAVANNA_PLATEAU, BiomeIds::FOREST, BiomeIds::FOREST,
             BiomeIds::JUNGLE},
            {BiomeIds::MESA, BiomeIds::MESA, BiomeIds::MESA, BiomeIds::MESA_PLATEAU_STONE,
             BiomeIds::MESA_PLATEAU_STONE}};

    const int32_t PLATEAU_BIOMES_VARIANT[5][5] = {
            {BiomeIds::ICE_PLAINS_SPIKES, NONE, NONE, NONE, NONE},
            {BiomeIds::CHERRY_GROVE, NONE, BiomeIds::MEADOW, BiomeIds::MEADOW, BiomeIds::MEGA_TAIGA},
            {BiomeIds::CHERRY_GROVE, BiomeIds::CHERRY_GROVE, BiomeIds::FOREST, BiomeIds::BIRCH_FOREST, NONE},
            {NONE, NONE, NONE, NONE, NONE},
            {BiomeIds::MESA_BRYCE, BiomeIds::MESA_BRYCE, NONE, NONE, NONE}};

    const int32_t SHATTERED_BIOMES[5][5] = {
            {BiomeIds::EXTREME_HILLS_MUTATED, BiomeIds::EXTREME_HILLS_MUTATED, BiomeIds::EXTREME_HILLS,
             BiomeIds::EXTREME_HILLS_PLUS_TREES, BiomeIds::EXTREME_HILLS_PLUS_TREES},
            {BiomeIds::EXTREME_HILLS_MUTATED, BiomeIds::EXTREME_HILLS_MUTATED, BiomeIds::EXTREME_HILLS,
             BiomeIds::EXTREME_HILLS_PLUS_TREES, BiomeIds::EXTREME_HILLS_PLUS_TREES},
            {BiomeIds::EXTREME_HILLS, BiomeIds::EXTREME_HILLS, BiomeIds::EXTREME_HILLS,
             BiomeIds::EXTREME_HILLS_PLUS_TREES, BiomeIds::EXTREME_HILLS_PLUS_TREES},
            {NONE, NONE, NONE, NONE, NONE},
            {NONE, NONE, NONE, NONE, NONE}};

    Parameter span(float min, float max) {
        return Parameter{OverworldBiomeParameters::quantize(min), OverworldBiomeParameters::quantize(max)};
    }

    Parameter span(const Parameter &min, const Parameter &max) {
        return Parameter{min.mMin, max.mMax};
    }

    Parameter point(float value) {
        return span(value, value);
    }

    // ranges from the static initializers next to sub_14718E740
    const Parameter FULL_RANGE = span(-1.0f, 1.0f);
    const Parameter TEMPERATURES[5] = {span(-1.0f, -0.45f), span(-0.45f, -0.15f), span(-0.15f, 0.2f), span(0.2f, 0.55f),
                                       span(0.55f, 1.0f)};
    const Parameter HUMIDITIES[5] = {span(-1.0f, -0.35f), span(-0.35f, -0.1f), span(-0.1f, 0.1f), span(0.1f, 0.3f),
                                     span(0.3f, 1.0f)};
    const Parameter EROSIONS[7] = {span(-1.0f, -0.78f),   span(-0.78f, -0.375f), span(-0.375f, -0.2225f),
                                   span(-0.2225f, 0.05f), span(0.05f, 0.45f),    span(0.45f, 0.55f),
                                   span(0.55f, 1.0f)};
    const Parameter FROZEN_RANGE = TEMPERATURES[0];
    const Parameter UNFROZEN_RANGE = span(TEMPERATURES[1], TEMPERATURES[4]);
    const Parameter MUSHROOM_FIELDS_CONTINENTALNESS = span(-1.2f, -1.05f);
    const Parameter DEEP_OCEAN_CONTINENTALNESS = span(-1.05f, -0.455f);
    const Parameter OCEAN_CONTINENTALNESS = span(-0.455f, -0.19f);
    const Parameter COAST_CONTINENTALNESS = span(-0.19f, -0.11f);
    const Parameter INLAND_CONTINENTALNESS = span(-0.11f, 0.55f);
    const Parameter NEAR_INLAND_CONTINENTALNESS = span(-0.11f, 0.03f);
    const Parameter MID_INLAND_CONTINENTALNESS = span(0.03f, 0.3f);
    const Parameter FAR_INLAND_CONTINENTALNESS = span(0.3f, 1.0f);

    int64_t middle(const Parameter &parameter) {
        return (parameter.mMin + parameter.mMax) / 2;
    }

    Space unionOf(const std::vector<std::unique_ptr<Node>> &nodes) {
        Space space = nodes.front()->mSpace;
        for (const std::unique_ptr<Node> &node: nodes) {
            for (int32_t i = 0; i < OverworldBiomeParameters::DIMENSIONS; i++) {
                space[(size_t) i].mMin = std::min(space[(size_t) i].mMin, node->mSpace[(size_t) i].mMin);
                space[(size_t) i].mMax = std::max(space[(size_t) i].mMax, node->mSpace[(size_t) i].mMax);
            }
        }

        return space;
    }

    std::unique_ptr<Node> subtree(std::vector<std::unique_ptr<Node>> children) {
        std::unique_ptr<Node> node = std::make_unique<Node>();
        node->mSpace = unionOf(children);
        node->mChildren = std::move(children);
        return node;
    }

    // ordering on one dimension, then the next ones in turn
    void sortNodes(std::vector<std::unique_ptr<Node>> &nodes, int32_t dimension, bool absolute) {
        std::stable_sort(nodes.begin(), nodes.end(),
                         [dimension, absolute](const std::unique_ptr<Node> &a, const std::unique_ptr<Node> &b) {
                             for (int32_t i = 0; i < OverworldBiomeParameters::DIMENSIONS; i++) {
                                 const size_t index = (size_t) ((dimension + i) % OverworldBiomeParameters::DIMENSIONS);
                                 int64_t first = middle(a->mSpace[index]);
                                 int64_t second = middle(b->mSpace[index]);
                                 if (absolute) {
                                     first = std::llabs(first);
                                     second = std::llabs(second);
                                 }

                                 if (first != second)
                                     return first < second;
                             }

                             return false;
                         });
    }

    int64_t cost(const Space &space) {
        int64_t total = 0;
        for (const Parameter &parameter: space)
            total += std::llabs(parameter.mMax - parameter.mMin);

        return total;
    }

    std::unique_ptr<Node> build(std::vector<std::unique_ptr<Node>> nodes) {
        if (nodes.size() == 1)
            return std::move(nodes.front());

        if (nodes.size() <= CHILDREN_PER_NODE) {
            std::stable_sort(nodes.begin(), nodes.end(),
                             [](const std::unique_ptr<Node> &a, const std::unique_ptr<Node> &b) {
                                 int64_t first = 0;
                                 int64_t second = 0;
                                 for (int32_t i = 0; i < OverworldBiomeParameters::DIMENSIONS; i++) {
                                     first += std::llabs(middle(a->mSpace[(size_t) i]));
                                     second += std::llabs(middle(b->mSpace[(size_t) i]));
                                 }

                                 return first < second;
                             });
            return subtree(std::move(nodes));
        }

        const size_t bucketSize = (size_t) std::pow(6.0,
                                                    std::floor(std::log((double) nodes.size() - 0.01) / std::log(6.0)));
        int64_t bestCost = std::numeric_limits<int64_t>::max();
        int32_t bestDimension = -1;
        std::vector<Node *> bestOrder;
        for (int32_t dimension = 0; dimension < OverworldBiomeParameters::DIMENSIONS; dimension++) {
            sortNodes(nodes, dimension, false);
            int64_t total = 0;
            for (size_t start = 0; start < nodes.size(); start += bucketSize) {
                Space space = nodes[start]->mSpace;
                for (size_t i = start; i < std::min(nodes.size(), start + bucketSize); i++) {
                    for (int32_t d = 0; d < OverworldBiomeParameters::DIMENSIONS; d++) {
                        space[(size_t) d].mMin = std::min(space[(size_t) d].mMin, nodes[i]->mSpace[(size_t) d].mMin);
                        space[(size_t) d].mMax = std::max(space[(size_t) d].mMax, nodes[i]->mSpace[(size_t) d].mMax);
                    }
                }

                total += cost(space);
            }

            if (bestCost > total) {
                bestCost = total;
                bestDimension = dimension;
                bestOrder.clear();
                for (const std::unique_ptr<Node> &node: nodes)
                    bestOrder.push_back(node.get());
            }
        }

        std::vector<std::unique_ptr<Node>> owned;
        for (Node *node: bestOrder) {
            for (std::unique_ptr<Node> &candidate: nodes) {
                if (candidate.get() == node) {
                    owned.push_back(std::move(candidate));
                    break;
                }
            }
        }

        std::vector<std::unique_ptr<Node>> buckets;
        for (size_t start = 0; start < owned.size(); start += bucketSize) {
            std::vector<std::unique_ptr<Node>> children;
            for (size_t i = start; i < std::min(owned.size(), start + bucketSize); i++)
                children.push_back(std::move(owned[i]));

            buckets.push_back(subtree(std::move(children)));
        }

        sortNodes(buckets, bestDimension, true);
        std::vector<std::unique_ptr<Node>> children;
        for (std::unique_ptr<Node> &bucket: buckets)
            children.push_back(build(std::move(bucket->mChildren)));

        return subtree(std::move(children));
    }

    int64_t distance(const Space &space, const OverworldBiomeParameters::Target &target) {
        int64_t total = 0;
        for (size_t i = 0; i < space.size(); i++) {
            int64_t difference = 0;
            if (target[i] > space[i].mMax)
                difference = target[i] - space[i].mMax;
            else if (target[i] < space[i].mMin)
                difference = space[i].mMin - target[i];

            total += difference * difference;
        }

        return total;
    }

    void searchChildren(const Node &node, const OverworldBiomeParameters::Target &target, const Node *&best,
                        int64_t &bestDistance) {
        for (const std::unique_ptr<Node> &child: node.mChildren) {
            const int64_t childDistance = distance(child->mSpace, target);
            if (childDistance >= bestDistance)
                continue;

            if (child->isLeaf()) {
                best = child.get();
                bestDistance = childDistance;
            } else {
                searchChildren(*child, target, best, bestDistance);
            }
        }
    }

    // sub_14CDF2910 with the slices sub_14CDB9F30, sub_14CDC0FC0, sub_14CDC6D90 and sub_14CDCBF50, for a new world
    // with every vanilla biome
    class Builder {
    public:
        explicit Builder(std::function<void(const Space &, int32_t)> add) : mAdd(std::move(add)) {}

        void addBiomes() {
            _addOffCoastBiomes();
            _addInlandBiomes();
            _addUndergroundBiomes();
        }

    private:
        void _addSurface(const Parameter &temperature, const Parameter &humidity, const Parameter &continentalness,
                         const Parameter &erosion, const Parameter &weirdness, int32_t biome) {
            mAdd(Space{temperature, humidity, continentalness, erosion, point(0.0f), weirdness, point(0.0f)}, biome);
            mAdd(Space{temperature, humidity, continentalness, erosion, point(1.0f), weirdness, point(0.0f)}, biome);
        }

        void _addUnderground(const Parameter &temperature, const Parameter &humidity, const Parameter &continentalness,
                             const Parameter &erosion, const Parameter &depth, const Parameter &weirdness,
                             int32_t biome) {
            mAdd(Space{temperature, humidity, continentalness, erosion, depth, weirdness, point(0.0f)}, biome);
        }

        void _addOffCoastBiomes() {
            _addSurface(FULL_RANGE, FULL_RANGE, MUSHROOM_FIELDS_CONTINENTALNESS, FULL_RANGE, FULL_RANGE,
                        BiomeIds::MUSHROOM_ISLAND);
            for (int32_t i = 0; i < 5; i++) {
                _addSurface(TEMPERATURES[i], FULL_RANGE, DEEP_OCEAN_CONTINENTALNESS, FULL_RANGE, FULL_RANGE,
                            OCEANS[0][i]);
                _addSurface(TEMPERATURES[i], FULL_RANGE, OCEAN_CONTINENTALNESS, FULL_RANGE, FULL_RANGE, OCEANS[1][i]);
            }
        }

        void _addInlandBiomes() {
            _addMidSlice(span(-1.0f, -0.93333334f));
            _addHighSlice(span(-0.93333334f, -0.7666667f));
            _addPeaks(span(-0.7666667f, -0.56666666f));
            _addHighSlice(span(-0.56666666f, -0.4f));
            _addMidSlice(span(-0.4f, -0.26666668f));
            _addLowSlice(span(-0.26666668f, -0.05f));
            _addValleys(span(-0.05f, 0.05f));
            _addLowSlice(span(0.05f, 0.26666668f));
            _addMidSlice(span(0.26666668f, 0.4f));
            _addHighSlice(span(0.4f, 0.56666666f));
            _addPeaks(span(0.56666666f, 0.7666667f));
            _addHighSlice(span(0.7666667f, 0.93333334f));
            _addMidSlice(span(0.93333334f, 1.0f));
        }

        void _addSwamps(const Parameter &weirdness) {
            const Parameter inland = span(NEAR_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS);
            _addSurface(span(TEMPERATURES[1], TEMPERATURES[2]), FULL_RANGE, inland, EROSIONS[6], weirdness,
                        BiomeIds::SWAMPLAND);
            _addSurface(span(TEMPERATURES[3], TEMPERATURES[4]), FULL_RANGE, inland, EROSIONS[6], weirdness,
                        BiomeIds::MANGROVE_SWAMP);
        }

        void _addPeaks(const Parameter &weirdness) {
            for (int32_t i = 0; i < 5; i++) {
                for (int32_t j = 0; j < 5; j++) {
                    const Parameter &t = TEMPERATURES[i];
                    const Parameter &h = HUMIDITIES[j];
                    const int32_t middleBiome = _pickMiddleBiome(i, j, weirdness);
                    const int32_t middleOrBadlands = _pickMiddleBiomeOrBadlandsIfHot(i, j, weirdness);
                    const int32_t middleOrBadlandsOrSlope = _pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(i, j,
                                                                                                         weirdness);
                    const int32_t plateau = _pickPlateauBiome(i, j, weirdness);
                    const int32_t shattered = _pickShatteredBiome(i, j, weirdness);
                    const int32_t shatteredSavanna = _maybePickWindsweptSavannaBiome(i, j, weirdness, shattered);
                    const int32_t peak = _pickPeakBiome(i, j, weirdness);
                    _addSurface(t, h, span(COAST_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[0], weirdness,
                                peak);
                    _addSurface(t, h, span(COAST_CONTINENTALNESS, NEAR_INLAND_CONTINENTALNESS), EROSIONS[1], weirdness,
                                middleOrBadlandsOrSlope);
                    _addSurface(t, h, span(MID_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[1],
                                weirdness, peak);
                    _addSurface(t, h, span(COAST_CONTINENTALNESS, NEAR_INLAND_CONTINENTALNESS),
                                span(EROSIONS[2], EROSIONS[3]), weirdness, middleBiome);
                    _addSurface(t, h, span(MID_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[2],
                                weirdness, plateau);
                    _addSurface(t, h, MID_INLAND_CONTINENTALNESS, EROSIONS[3], weirdness, middleOrBadlands);
                    _addSurface(t, h, FAR_INLAND_CONTINENTALNESS, EROSIONS[3], weirdness, plateau);
                    _addSurface(t, h, span(COAST_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[4], weirdness,
                                middleBiome);
                    _addSurface(t, h, span(COAST_CONTINENTALNESS, NEAR_INLAND_CONTINENTALNESS), EROSIONS[5], weirdness,
                                shatteredSavanna);
                    _addSurface(t, h, span(MID_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[5],
                                weirdness, shattered);
                    _addSurface(t, h, span(COAST_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[6], weirdness,
                                middleBiome);
                }
            }
        }

        void _addHighSlice(const Parameter &weirdness) {
            for (int32_t i = 0; i < 5; i++) {
                for (int32_t j = 0; j < 5; j++) {
                    const Parameter &t = TEMPERATURES[i];
                    const Parameter &h = HUMIDITIES[j];
                    const int32_t middleBiome = _pickMiddleBiome(i, j, weirdness);
                    const int32_t middleOrBadlands = _pickMiddleBiomeOrBadlandsIfHot(i, j, weirdness);
                    const int32_t middleOrBadlandsOrSlope = _pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(i, j,
                                                                                                         weirdness);
                    const int32_t plateau = _pickPlateauBiome(i, j, weirdness);
                    const int32_t shattered = _pickShatteredBiome(i, j, weirdness);
                    const int32_t middleSavanna = _maybePickWindsweptSavannaBiome(i, j, weirdness, middleBiome);
                    const int32_t slope = _pickSlopeBiome(i, j, weirdness);
                    const int32_t peak = _pickPeakBiome(i, j, weirdness);
                    _addSurface(t, h, COAST_CONTINENTALNESS, span(EROSIONS[0], EROSIONS[1]), weirdness, middleBiome);
                    _addSurface(t, h, NEAR_INLAND_CONTINENTALNESS, EROSIONS[0], weirdness, slope);
                    _addSurface(t, h, span(MID_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[0],
                                weirdness, peak);
                    _addSurface(t, h, NEAR_INLAND_CONTINENTALNESS, EROSIONS[1], weirdness, middleOrBadlandsOrSlope);
                    _addSurface(t, h, span(MID_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[1],
                                weirdness, slope);
                    _addSurface(t, h, span(COAST_CONTINENTALNESS, NEAR_INLAND_CONTINENTALNESS),
                                span(EROSIONS[2], EROSIONS[3]), weirdness, middleBiome);
                    _addSurface(t, h, span(MID_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[2],
                                weirdness, plateau);
                    _addSurface(t, h, MID_INLAND_CONTINENTALNESS, EROSIONS[3], weirdness, middleOrBadlands);
                    _addSurface(t, h, FAR_INLAND_CONTINENTALNESS, EROSIONS[3], weirdness, plateau);
                    _addSurface(t, h, span(COAST_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[4], weirdness,
                                middleBiome);
                    _addSurface(t, h, span(COAST_CONTINENTALNESS, NEAR_INLAND_CONTINENTALNESS), EROSIONS[5], weirdness,
                                middleSavanna);
                    _addSurface(t, h, span(MID_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[5],
                                weirdness, shattered);
                    _addSurface(t, h, span(COAST_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[6], weirdness,
                                middleBiome);
                }
            }
        }

        void _addMidSlice(const Parameter &weirdness) {
            _addSurface(FULL_RANGE, FULL_RANGE, COAST_CONTINENTALNESS, span(EROSIONS[0], EROSIONS[2]), weirdness,
                        BiomeIds::STONE_BEACH);
            _addSwamps(weirdness);
            for (int32_t i = 0; i < 5; i++) {
                for (int32_t j = 0; j < 5; j++) {
                    const Parameter &t = TEMPERATURES[i];
                    const Parameter &h = HUMIDITIES[j];
                    const int32_t middleBiome = _pickMiddleBiome(i, j, weirdness);
                    const int32_t middleOrBadlands = _pickMiddleBiomeOrBadlandsIfHot(i, j, weirdness);
                    const int32_t middleOrBadlandsOrSlope = _pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(i, j,
                                                                                                         weirdness);
                    const int32_t shattered = _pickShatteredBiome(i, j, weirdness);
                    const int32_t plateau = _pickPlateauBiome(i, j, weirdness);
                    const int32_t beach = _pickBeachBiome(i, j);
                    const int32_t middleSavanna = _maybePickWindsweptSavannaBiome(i, j, weirdness, middleBiome);
                    const int32_t shatteredCoast = _pickShatteredCoastBiome(i, j, weirdness);
                    _addSurface(t, h, span(NEAR_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[0],
                                weirdness, middleOrBadlandsOrSlope);
                    _addSurface(t, h, span(NEAR_INLAND_CONTINENTALNESS, MID_INLAND_CONTINENTALNESS), EROSIONS[1],
                                weirdness, middleOrBadlands);
                    _addSurface(t, h, FAR_INLAND_CONTINENTALNESS, EROSIONS[1], weirdness,
                                i == 0 ? plateau : middleOrBadlandsOrSlope);
                    _addSurface(t, h, NEAR_INLAND_CONTINENTALNESS, EROSIONS[2], weirdness, middleBiome);
                    _addSurface(t, h, MID_INLAND_CONTINENTALNESS, EROSIONS[2], weirdness, middleOrBadlands);
                    _addSurface(t, h, FAR_INLAND_CONTINENTALNESS, EROSIONS[2], weirdness, plateau);
                    _addSurface(t, h, span(COAST_CONTINENTALNESS, NEAR_INLAND_CONTINENTALNESS), EROSIONS[3], weirdness,
                                middleBiome);
                    _addSurface(t, h, span(MID_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[3],
                                weirdness, middleOrBadlands);
                    if (weirdness.mMax < 0) {
                        _addSurface(t, h, COAST_CONTINENTALNESS, EROSIONS[4], weirdness, beach);
                        _addSurface(t, h, span(NEAR_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[4],
                                    weirdness, middleBiome);
                    } else {
                        _addSurface(t, h, span(COAST_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[4],
                                    weirdness, middleBiome);
                    }

                    _addSurface(t, h, COAST_CONTINENTALNESS, EROSIONS[5], weirdness, shatteredCoast);
                    _addSurface(t, h, NEAR_INLAND_CONTINENTALNESS, EROSIONS[5], weirdness, middleSavanna);
                    _addSurface(t, h, span(MID_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[5],
                                weirdness, shattered);
                    _addSurface(t, h, COAST_CONTINENTALNESS, EROSIONS[6], weirdness,
                                weirdness.mMax < 0 ? beach : middleBiome);
                    if (i == 0) {
                        _addSurface(t, h, span(NEAR_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[6],
                                    weirdness, middleBiome);
                    }
                }
            }
        }

        void _addLowSlice(const Parameter &weirdness) {
            _addSurface(FULL_RANGE, FULL_RANGE, COAST_CONTINENTALNESS, span(EROSIONS[0], EROSIONS[2]), weirdness,
                        BiomeIds::STONE_BEACH);
            _addSwamps(weirdness);
            for (int32_t i = 0; i < 5; i++) {
                for (int32_t j = 0; j < 5; j++) {
                    const Parameter &t = TEMPERATURES[i];
                    const Parameter &h = HUMIDITIES[j];
                    const int32_t middleBiome = _pickMiddleBiome(i, j, weirdness);
                    const int32_t middleOrBadlands = _pickMiddleBiomeOrBadlandsIfHot(i, j, weirdness);
                    const int32_t middleOrBadlandsOrSlope = _pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(i, j,
                                                                                                         weirdness);
                    const int32_t beach = _pickBeachBiome(i, j);
                    const int32_t middleSavanna = _maybePickWindsweptSavannaBiome(i, j, weirdness, middleBiome);
                    const int32_t shatteredCoast = _pickShatteredCoastBiome(i, j, weirdness);
                    _addSurface(t, h, NEAR_INLAND_CONTINENTALNESS, span(EROSIONS[0], EROSIONS[1]), weirdness,
                                middleOrBadlands);
                    _addSurface(t, h, span(MID_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS),
                                span(EROSIONS[0], EROSIONS[1]), weirdness, middleOrBadlandsOrSlope);
                    _addSurface(t, h, NEAR_INLAND_CONTINENTALNESS, span(EROSIONS[2], EROSIONS[3]), weirdness,
                                middleBiome);
                    _addSurface(t, h, span(MID_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS),
                                span(EROSIONS[2], EROSIONS[3]), weirdness, middleOrBadlands);
                    _addSurface(t, h, COAST_CONTINENTALNESS, span(EROSIONS[3], EROSIONS[4]), weirdness, beach);
                    _addSurface(t, h, span(NEAR_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[4],
                                weirdness, middleBiome);
                    _addSurface(t, h, COAST_CONTINENTALNESS, EROSIONS[5], weirdness, shatteredCoast);
                    _addSurface(t, h, NEAR_INLAND_CONTINENTALNESS, EROSIONS[5], weirdness, middleSavanna);
                    _addSurface(t, h, span(MID_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[5],
                                weirdness, middleBiome);
                    _addSurface(t, h, COAST_CONTINENTALNESS, EROSIONS[6], weirdness, beach);
                    if (i == 0) {
                        _addSurface(t, h, span(NEAR_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), EROSIONS[6],
                                    weirdness, middleBiome);
                    }
                }
            }
        }

        void _addValleys(const Parameter &weirdness) {
            const Parameter shores = span(EROSIONS[0], EROSIONS[1]);
            const bool negative = weirdness.mMax < 0;
            _addSurface(FROZEN_RANGE, FULL_RANGE, COAST_CONTINENTALNESS, shores, weirdness,
                        negative ? BiomeIds::STONE_BEACH : BiomeIds::FROZEN_RIVER);
            _addSurface(UNFROZEN_RANGE, FULL_RANGE, COAST_CONTINENTALNESS, shores, weirdness,
                        negative ? BiomeIds::STONE_BEACH : BiomeIds::RIVER);
            _addSurface(FROZEN_RANGE, FULL_RANGE, NEAR_INLAND_CONTINENTALNESS, shores, weirdness,
                        BiomeIds::FROZEN_RIVER);
            _addSurface(UNFROZEN_RANGE, FULL_RANGE, NEAR_INLAND_CONTINENTALNESS, shores, weirdness, BiomeIds::RIVER);
            const Parameter coastToFar = span(COAST_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS);
            _addSurface(FROZEN_RANGE, FULL_RANGE, coastToFar, span(EROSIONS[2], EROSIONS[5]), weirdness,
                        BiomeIds::FROZEN_RIVER);
            _addSurface(UNFROZEN_RANGE, FULL_RANGE, coastToFar, span(EROSIONS[2], EROSIONS[5]), weirdness,
                        BiomeIds::RIVER);
            _addSurface(FROZEN_RANGE, FULL_RANGE, COAST_CONTINENTALNESS, EROSIONS[6], weirdness,
                        BiomeIds::FROZEN_RIVER);
            _addSurface(UNFROZEN_RANGE, FULL_RANGE, COAST_CONTINENTALNESS, EROSIONS[6], weirdness, BiomeIds::RIVER);
            const Parameter inlandToFar = span(INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS);
            _addSurface(span(TEMPERATURES[1], TEMPERATURES[2]), FULL_RANGE, inlandToFar, EROSIONS[6], weirdness,
                        BiomeIds::SWAMPLAND);
            _addSurface(span(TEMPERATURES[3], TEMPERATURES[4]), FULL_RANGE, inlandToFar, EROSIONS[6], weirdness,
                        BiomeIds::MANGROVE_SWAMP);
            _addSurface(FROZEN_RANGE, FULL_RANGE, inlandToFar, EROSIONS[6], weirdness, BiomeIds::FROZEN_RIVER);
            for (int32_t i = 0; i < 5; i++) {
                for (int32_t j = 0; j < 5; j++) {
                    _addSurface(TEMPERATURES[i], HUMIDITIES[j],
                                span(MID_INLAND_CONTINENTALNESS, FAR_INLAND_CONTINENTALNESS), shores, weirdness,
                                _pickMiddleBiomeOrBadlandsIfHot(i, j, weirdness));
                }
            }
        }

        void _addUndergroundBiomes() {
            // sub_14CDF2910: the cave biomes and their ranges (ice caves need an experiment)
            const Parameter caveDepth = span(0.2f, 0.9f);
            _addUnderground(FULL_RANGE, span(-1.0f, 0.7f), span(0.8f, 1.0f), FULL_RANGE, caveDepth, FULL_RANGE,
                            BiomeIds::DRIPSTONE_CAVES);
            _addUnderground(FULL_RANGE, span(0.7f, 1.0f), FULL_RANGE, FULL_RANGE, caveDepth, FULL_RANGE,
                            BiomeIds::LUSH_CAVES);
            _addUnderground(FULL_RANGE, span(-1.0f, 0.7f), span(COAST_CONTINENTALNESS, INLAND_CONTINENTALNESS),
                            span(EROSIONS[5], EROSIONS[6]), caveDepth, span(-1.1f, -0.85f), BiomeIds::SULFUR_CAVES);
            _addUnderground(FULL_RANGE, FULL_RANGE, FULL_RANGE, span(EROSIONS[0], EROSIONS[1]), point(1.1f), FULL_RANGE,
                            BiomeIds::DEEP_DARK);
        }

        int32_t _pickMiddleBiome(int32_t temperature, int32_t humidity, const Parameter &weirdness) const {
            if (weirdness.mMax < 0)
                return MIDDLE_BIOMES[temperature][humidity];

            const int32_t variant = MIDDLE_BIOMES_VARIANT[temperature][humidity];
            return variant == NONE ? MIDDLE_BIOMES[temperature][humidity] : variant;
        }

        int32_t _pickMiddleBiomeOrBadlandsIfHot(int32_t temperature, int32_t humidity,
                                                const Parameter &weirdness) const {
            return temperature == 4 ? _pickBadlandsBiome(humidity, weirdness)
                                    : _pickMiddleBiome(temperature, humidity, weirdness);
        }

        int32_t _pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(int32_t temperature, int32_t humidity,
                                                             const Parameter &weirdness) const {
            return temperature == 0 ? _pickSlopeBiome(temperature, humidity, weirdness)
                                    : _pickMiddleBiomeOrBadlandsIfHot(temperature, humidity, weirdness);
        }

        int32_t _maybePickWindsweptSavannaBiome(int32_t temperature, int32_t humidity, const Parameter &weirdness,
                                                int32_t biome) const {
            return temperature > 1 && humidity < 4 && weirdness.mMax >= 0 ? BiomeIds::SAVANNA_MUTATED : biome;
        }

        int32_t _pickShatteredCoastBiome(int32_t temperature, int32_t humidity, const Parameter &weirdness) const {
            const int32_t biome = weirdness.mMax >= 0 ? _pickMiddleBiome(temperature, humidity, weirdness)
                                                      : _pickBeachBiome(temperature, humidity);
            return _maybePickWindsweptSavannaBiome(temperature, humidity, weirdness, biome);
        }

        static int32_t _pickBeachBiome(int32_t temperature, int32_t) {
            if (temperature == 0)
                return BiomeIds::COLD_BEACH;

            return temperature == 4 ? BiomeIds::DESERT : BiomeIds::BEACH;
        }

        static int32_t _pickBadlandsBiome(int32_t humidity, const Parameter &weirdness) {
            if (humidity < 2)
                return weirdness.mMax < 0 ? BiomeIds::MESA : BiomeIds::MESA_BRYCE;

            return humidity < 3 ? BiomeIds::MESA : BiomeIds::MESA_PLATEAU_STONE;
        }

        int32_t _pickPlateauBiome(int32_t temperature, int32_t humidity, const Parameter &weirdness) const {
            if (weirdness.mMax >= 0) {
                const int32_t variant = PLATEAU_BIOMES_VARIANT[temperature][humidity];
                if (variant != NONE)
                    return variant;
            }

            return PLATEAU_BIOMES[temperature][humidity];
        }

        int32_t _pickPeakBiome(int32_t temperature, int32_t humidity, const Parameter &weirdness) const {
            if (temperature <= 2)
                return weirdness.mMax < 0 ? BiomeIds::JAGGED_PEAKS : BiomeIds::FROZEN_PEAKS;

            return temperature == 3 ? BiomeIds::STONY_PEAKS : _pickBadlandsBiome(humidity, weirdness);
        }

        int32_t _pickSlopeBiome(int32_t temperature, int32_t humidity, const Parameter &weirdness) const {
            if (temperature >= 3)
                return _pickPlateauBiome(temperature, humidity, weirdness);

            return humidity <= 1 ? BiomeIds::SNOWY_SLOPES : BiomeIds::GROVE;
        }

        int32_t _pickShatteredBiome(int32_t temperature, int32_t humidity, const Parameter &weirdness) const {
            const int32_t biome = SHATTERED_BIOMES[temperature][humidity];
            return biome == NONE ? _pickMiddleBiome(temperature, humidity, weirdness) : biome;
        }

        std::function<void(const Space &, int32_t)> mAdd;
    };

}

int64_t OverworldBiomeParameters::quantize(float value) {
    return (int64_t) ((double) value * 10000.0);
}

OverworldBiomeParameters::Target OverworldBiomeParameters::target(float temperature, float humidity,
                                                                  float continentalness, float erosion, float depth,
                                                                  float weirdness) {
    return Target{quantize(temperature),
                  quantize(humidity),
                  quantize(continentalness),
                  quantize(erosion),
                  quantize(depth),
                  quantize(weirdness),
                  0};
}

OverworldBiomeParameters::OverworldBiomeParameters() {
    Builder builder([this](const Space &space, int32_t biome) { _addEntry(space, biome); });
    builder.addBiomes();
    mRoot = build(std::move(mLeaves));
}

const OverworldBiomeParameters &OverworldBiomeParameters::shared() {
    static const OverworldBiomeParameters parameters;
    return parameters;
}

void OverworldBiomeParameters::_addEntry(const Space &space, int32_t biome) {
    std::unique_ptr<Node> leaf = std::make_unique<Node>();
    leaf->mSpace = space;
    leaf->mBiome = biome;
    mLeaves.push_back(std::move(leaf));
}

const OverworldBiomeParameters::Node *OverworldBiomeParameters::search(const Target &target,
                                                                       const Node *lastResult) const {
    if (mRoot->isLeaf())
        return mRoot.get();

    const Node *best = lastResult;
    int64_t bestDistance = lastResult == nullptr ? std::numeric_limits<int64_t>::max()
                                                 : distance(lastResult->mSpace, target);
    searchChildren(*mRoot, target, best, bestDistance);
    return best;
}
