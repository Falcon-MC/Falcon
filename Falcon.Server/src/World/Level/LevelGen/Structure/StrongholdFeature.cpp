#include "World/Level/LevelGen/Structure/StrongholdFeature.h"

#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Feature/FeatureMath.h"
#include "World/Level/LevelGen/Random/GameRandom.h"

#include <climits>
#include <cstdlib>
#include <map>
#include <mutex>
#include <random>
#include <utility>

namespace {
    // a cell's start lands 50 to 149 chunks into it
    const int32_t CELL_RANGE = 100;
    const int32_t CELL_MARGIN = 50;
    const int32_t CELL_SALT = 97858791;
    const uint32_t CELL_X_MULTIPLIER = 2611735377U;
    const uint32_t CELL_Z_MULTIPLIER = 3194531513U;
    // a float under 0.25f, compared as a double
    const double CELL_CHANCE = 0.2499999925494194;
    // squared chunks from the origin under which no grid stronghold is drawn
    const uint32_t ORIGIN_EXCLUSION = 100;

    const int32_t VILLAGE_STRONGHOLDS = 3;
    const int32_t SPIRAL_STEPS = 1000;
    const int32_t SPIRAL_FIRST_DISTANCE = 40;
    const int32_t SPIRAL_DISTANCE_RANGE = 16;
    const int32_t SPIRAL_STEP = 4;
    const int32_t SPIRAL_STEP_AFTER_FIND = 8;
    const float SPIRAL_TURN = 0.7853982f;
    const float SPIRAL_TURN_AFTER_FIND = 1.8849558f;
    const float FULL_TURN = 6.2831855f;
    const float SINE_SCALE = 10430.378f;
    const float QUARTER_TURN_SCALED = 16384.0f;
    // the spiral looks for a village start in the 16 by 16 chunks from 8 before its point
    const int32_t SPIRAL_WINDOW_BEFORE = 8;
    const int32_t SPIRAL_WINDOW_AFTER = 7;

    const int32_t SEARCH_RINGS = 100;

    const StrongholdFeature gStronghold;

    int32_t floorFloat(float value) {
        const int32_t truncated = (int32_t) value;
        return value < (float) truncated ? truncated - 1 : truncated;
    }

    uint32_t squaredLength(int32_t x, int32_t z) {
        return (uint32_t) x * (uint32_t) x + (uint32_t) z * (uint32_t) z;
    }

    bool sameCell(const StructureChunkPos &start, int32_t cellX, int32_t cellZ) {
        return StrongholdFeature::cellOf(start.x) == cellX && StrongholdFeature::cellOf(start.z) == cellZ;
    }

    std::vector<StructureChunkPos> spiral(const ChunkGenerator &generator) {
        std::vector<StructureChunkPos> found;
        const StructureFeature *village = StructureFeature::get("village");
        if (village == nullptr)
            return found;

        std::mt19937 random((uint32_t) generator.getSeed());
        float angle = GameRandom::nextFloat(random) * FULL_TURN;
        int32_t distance = GameRandom::nextInt(random, SPIRAL_DISTANCE_RANGE) + SPIRAL_FIRST_DISTANCE;
        for (int32_t step = 0; step < SPIRAL_STEPS; ++step) {
            const float scaled = angle * SINE_SCALE;
            const int32_t centerX = floorFloat((float) distance * FeatureMath::sineAt(scaled + QUARTER_TURN_SCALED));
            const int32_t centerZ = floorFloat((float) distance * FeatureMath::sineAt(scaled));

            bool hit = false;
            for (int32_t x = centerX - SPIRAL_WINDOW_BEFORE; x <= centerX + SPIRAL_WINDOW_AFTER && !hit; ++x) {
                for (int32_t z = centerZ - SPIRAL_WINDOW_BEFORE; z <= centerZ + SPIRAL_WINDOW_AFTER; ++z) {
                    if (!village->isStartChunk(generator, x, z))
                        continue;
                    found.push_back({x, z});
                    hit = true;
                    break;
                }
            }

            if ((int32_t) found.size() >= VILLAGE_STRONGHOLDS)
                break;
            distance += hit ? SPIRAL_STEP_AFTER_FIND : SPIRAL_STEP;
            angle += hit ? SPIRAL_TURN_AFTER_FIND : SPIRAL_TURN;
        }
        return found;
    }
}

StrongholdFeature::StrongholdFeature()
        : StructureFeature("stronghold", CELL_SIZE, CELL_SIZE - CELL_RANGE, CELL_SALT, Spread::Linear) {}

int32_t StrongholdFeature::cellOf(int32_t chunk) {
    return floorFloat((float) chunk / (float) CELL_SIZE);
}

std::shared_ptr<const std::vector<StructureChunkPos>>
StrongholdFeature::villageStrongholds(const ChunkGenerator &generator) {
    static std::mutex mutex;
    static std::map<int64_t, std::shared_ptr<const std::vector<StructureChunkPos>>> found;

    const int64_t seed = generator.getSeed();
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto known = found.find(seed);
        if (known != found.end())
            return known->second;
    }

    // walked unlocked: other workers keep generating
    auto starts = std::make_shared<const std::vector<StructureChunkPos>>(spiral(generator));
    std::lock_guard<std::mutex> lock(mutex);
    return found.emplace(seed, starts).first->second;
}

bool StrongholdFeature::gridStronghold(int64_t seed, int32_t centerX, int32_t centerZ, StructureChunkPos &out) {
    if (squaredLength(centerX, centerZ) < ORIGIN_EXCLUSION)
        return false;

    std::mt19937 random(CELL_Z_MULTIPLIER * (uint32_t) centerZ + CELL_X_MULTIPLIER * (uint32_t) centerX
                        + (uint32_t) seed + (uint32_t) CELL_SALT);
    const uint32_t offsetX = GameRandom::nextUnsigned(random) % (uint32_t) CELL_RANGE;
    const int32_t offsetZ = GameRandom::nextInt(random, CELL_RANGE);
    const double chance = (double) GameRandom::nextUnsigned(random) * 2.3283064365386963e-10;

    out.x = CELL_SIZE * cellOf(centerX) + (int32_t) offsetX + CELL_MARGIN;
    out.z = CELL_SIZE * cellOf(centerZ) + offsetZ + CELL_MARGIN;
    return chance < CELL_CHANCE;
}

StructureChunkPos StrongholdFeature::getCandidate(int64_t seed, int32_t regionX, int32_t regionZ) const {
    StructureChunkPos candidate;
    gridStronghold(seed, regionX * CELL_SIZE + CELL_SIZE / 2, regionZ * CELL_SIZE + CELL_SIZE / 2, candidate);
    return candidate;
}

bool StrongholdFeature::isStartChunk(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    for (const StructureChunkPos &start: *villageStrongholds(generator)) {
        if (start.x == chunkX && start.z == chunkZ)
            return true;
    }

    // a grid start may share its cell with a village one: the generation never checks
    if (squaredLength(chunkX, chunkZ) < ORIGIN_EXCLUSION)
        return false;

    StructureChunkPos start;
    return gridStronghold(generator.getSeed(), cellOf(chunkX) * CELL_SIZE + CELL_SIZE / 2,
                          cellOf(chunkZ) * CELL_SIZE + CELL_SIZE / 2, start)
           && start.x == chunkX && start.z == chunkZ;
}

bool StrongholdFeature::_isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    return isStartChunk(generator, chunkX, chunkZ);
}

bool StrongholdFeature::findNearest(const ChunkGenerator &generator, int32_t blockX, int32_t blockZ,
                                    StructureChunkPos &out) const {
    const int32_t originX = blockX >> 4;
    const int32_t originZ = blockZ >> 4;
    const int32_t originCellX = cellOf(originX);
    const int32_t originCellZ = cellOf(originZ);
    const std::shared_ptr<const std::vector<StructureChunkPos>> villages = villageStrongholds(generator);

    std::vector<std::pair<bool, StructureChunkPos>> found;
    for (const StructureChunkPos &start: *villages)
        found.emplace_back(true, start);

    // the village starts count from the first ring on, so the search ends there whenever one exists
    for (int32_t ring = 1; ring < SEARCH_RINGS; ++ring) {
        for (int32_t cellX = originCellX - ring; cellX <= originCellX + ring; ++cellX) {
            const bool edgeColumn = ring < 2 || std::abs(cellX - originCellX) >= ring;
            for (int32_t cellZ = originCellZ - ring; cellZ <= originCellZ + ring; ++cellZ) {
                if (!edgeColumn && std::abs(cellZ - originCellZ) < ring)
                    continue;

                // the search leaves out the cells of the village starts, which the generation does not
                bool villageCell = false;
                for (const StructureChunkPos &start: *villages)
                    villageCell = villageCell || sameCell(start, cellX, cellZ);
                if (villageCell)
                    continue;

                StructureChunkPos start;
                const bool valid = gridStronghold(generator.getSeed(), CELL_SIZE * cellX + CELL_SIZE / 2,
                                                  CELL_SIZE * cellZ + CELL_SIZE / 2, start);
                found.emplace_back(valid, valid ? start : StructureChunkPos{});
            }
        }

        int32_t best = INT_MAX;
        bool any = false;
        for (const std::pair<bool, StructureChunkPos> &entry: found) {
            if (!entry.first)
                continue;
            const int32_t distance = (int32_t) squaredLength(entry.second.x - originX, entry.second.z - originZ);
            if (distance >= best)
                continue;
            best = distance;
            out = entry.second;
            any = true;
        }
        if (any)
            return true;
    }
    return false;
}
