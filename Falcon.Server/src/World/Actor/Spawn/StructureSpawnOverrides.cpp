#include "World/Actor/Spawn/StructureSpawnOverrides.h"

#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"

namespace {
    const char *const FORTRESS = "minecraft:fortress";
    const char *const MONSTER = "monster";

    struct Entry {
        const char *mIdentifier;
        int32_t mWeight;
    };

    // the fortress's monster list in the client's structure spawn table; every entry spawns herds of 2 to 3
    const Entry FORTRESS_MONSTERS[] = {{"minecraft:zombie_pigman", 5},
                                       {"minecraft:magma_cube", 3},
                                       {"minecraft:blaze", 10},
                                       {"minecraft:skeleton", 2},
                                       {"minecraft:wither_skeleton", 8}};
    const int32_t FORTRESS_HERD_MIN = 2;
    const int32_t FORTRESS_HERD_MAX = 3;

    // the monster population never spawns on peaceful
    const int32_t MIN_MONSTER_DIFFICULTY = 1;

    std::vector<SpawnRule> buildFortress() {
        std::vector<SpawnRule> rules;
        for (const Entry &entry: FORTRESS_MONSTERS) {
            SpawnCondition condition;
            condition.mOnSurface = true;
            condition.mUnderground = true;
            condition.mWeight = entry.mWeight;
            condition.mHerds.push_back(SpawnHerd{FORTRESS_HERD_MIN, FORTRESS_HERD_MAX});
            condition.mMinDifficulty = MIN_MONSTER_DIFFICULTY;

            SpawnRule rule;
            rule.mIdentifier = entry.mIdentifier;
            rule.mPopulation = MONSTER;
            rule.mConditions.push_back(std::move(condition));
            rules.push_back(std::move(rule));
        }
        return rules;
    }
}

const std::vector<SpawnRule> *StructureSpawnOverrides::forStructure(const std::string &structure,
                                                                    const std::string &population) {
    static const std::vector<SpawnRule> fortress = buildFortress();
    if (structure == FORTRESS && population == MONSTER)
        return &fortress;
    return nullptr;
}

const std::vector<SpawnRule> *StructureSpawnOverrides::find(Level &level, const Vector3i &position,
                                                            const std::string &population) {
    LevelChunk *chunk = level.peekChunkPtr(position.x >> 4, position.z >> 4);
    if (chunk == nullptr || chunk->getStructureBounds().empty())
        return nullptr;

    // the piece boxes the chunk recorded, not the whole start
    for (const std::string &name: chunk->getStructureBounds().namesAt(position.x, position.y, position.z)) {
        if (const std::vector<SpawnRule> *rules = forStructure(name, population))
            return rules;
    }
    return nullptr;
}
