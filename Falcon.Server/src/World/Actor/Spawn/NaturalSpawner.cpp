#include "World/Actor/Spawn/NaturalSpawner.h"

#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LightSystem.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace {
    // Pool indices follow the population_control parser of BDS 26.60 (sub_142D9F9A0, switch at 0x142DA2D95).
    const char *const POOLS[NaturalSpawner::POOL_COUNT] = {
            "animal", "monster", "water_animal", "villager", "ambient", "cat", "pillager"
    };

    // Written by the BDS 26.60 Dimension constructor (sub_142930ED0, 0x142932480-0x1429324F0) and read by the
    // spawner at 0x1432C21A8: the surface row lives at Dimension+0x104, the underground row at Dimension+0x120.
    const int32_t POOL_CAPS[2][NaturalSpawner::POOL_COUNT] = {
            {4, 16, 36, 0, 2, 0, 8},
            {4, 8, 36, 0, 0, 4, 8}
    };

    const int32_t CHUNK_SPAWN_ROLL = 2000;
    const int32_t CHUNK_SPAWN_THRESHOLD = 10;
    const int32_t COUNT_CHUNK_RADIUS = 4;
    const int32_t MAX_SPAWNED_MOBS = 200;
    const float HERD_OFFSET = 0.49f;
    const int32_t TICKS_PER_SECOND = 20;

    int32_t poolIndex(const std::string &population) {
        for (int32_t index = 0; index < NaturalSpawner::POOL_COUNT; ++index) {
            if (population == POOLS[index])
                return index;
        }
        return -1;
    }

    bool isSolid(const BlockState &state) {
        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        return data != nullptr && data->mSolid;
    }

    bool isPassable(const BlockState &state) {
        const LiquidView liquid(state);
        return !isSolid(state) && !liquid.isLiquid() && !liquid.isBubbleColumn();
    }

    bool testFilter(const json::Value &filter, int32_t biome, bool snowCovered);

    bool testAll(const json::Value &filters, int32_t biome, bool snowCovered) {
        if (!filters.isArray())
            return testFilter(filters, biome, snowCovered);

        for (const std::unique_ptr<json::Value> &entry: filters.mArray) {
            if (!testFilter(*entry, biome, snowCovered))
                return false;
        }
        return true;
    }

    bool testAny(const json::Value &filters, int32_t biome, bool snowCovered) {
        if (!filters.isArray())
            return testFilter(filters, biome, snowCovered);

        for (const std::unique_ptr<json::Value> &entry: filters.mArray) {
            if (testFilter(*entry, biome, snowCovered))
                return true;
        }
        return false;
    }

    bool testFilter(const json::Value &filter, int32_t biome, bool snowCovered) {
        if (filter.isArray())
            return testAll(filter, biome, snowCovered);
        if (const json::Value *all = filter.get("all_of"))
            return testAll(*all, biome, snowCovered);
        if (const json::Value *any = filter.get("any_of"))
            return testAny(*any, biome, snowCovered);
        if (const json::Value *none = filter.get("none_of"))
            return !testAny(*none, biome, snowCovered);

        const json::Value *test = filter.get("test");
        if (test == nullptr)
            return true;

        const json::Value *operation = filter.get("operator");
        const std::string op = operation == nullptr ? "==" : operation->string();
        const bool negate = op == "!=" || op == "not" || op == "<>";
        const json::Value *value = filter.get("value");

        bool result;
        if (test->string() == "has_biome_tag")
            result = value != nullptr && BiomeChunkGenDataRegistry::hasTag(biome, value->string());
        else if (test->string() == "is_snow_covered")
            result = snowCovered == (value == nullptr || value->boolean(true));
        else
            return false;

        return negate ? !result : result;
    }

    std::string permute(const SpawnCondition &condition, const std::string &identifier, int32_t roll) {
        for (const SpawnPermutation &permutation: condition.mPermutations) {
            roll -= permutation.mWeight;
            if (roll < 0)
                return permutation.mIdentifier.empty() ? identifier : permutation.mIdentifier;
        }
        return identifier;
    }

    int32_t permutationWeight(const SpawnCondition &condition) {
        int32_t total = 0;
        for (const SpawnPermutation &permutation: condition.mPermutations)
            total += std::max(0, permutation.mWeight);
        return total;
    }
}

int32_t NaturalSpawner::_nextInt(int32_t bound) {
    if (bound <= 1)
        return 0;

    std::uniform_int_distribution<int32_t> distribution(0, bound - 1);
    return distribution(mRandom);
}

float NaturalSpawner::_nextFloat() {
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    return distribution(mRandom);
}

void NaturalSpawner::tick(ServerNetworkHandler &owner, Level &level) {
    std::vector<Vector3f> players;
    for (auto &entry: owner.getPlayers()) {
        const ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || !player.isIn(level)
            || player.getGameType() == (int32_t) GameType::Spectator)
            continue;
        players.push_back(player.getPosition());
    }
    if (players.empty())
        return;

    _despawn(owner, level, players);

    if (!level.getGameRules().getBool("domobspawning"))
        return;

    int32_t mobCount = 0;
    for (auto &entry: owner.getActors()) {
        const ServerActor &actor = *entry.second;
        if (actor.isAlive() && !SpawnRules::getPopulation(actor.getIdentifier()).empty())
            ++mobCount;
    }

    const int32_t difficulty = (int32_t) owner.getProperties().getDifficulty();
    const std::vector<int64_t> columns(level.getActiveColumns().begin(), level.getActiveColumns().end());
    for (const int64_t column: columns) {
        // A ticking chunk only goes on when nextInt(2000) <= 10 (0x1432C3021).
        if (_nextInt(CHUNK_SPAWN_ROLL) > CHUNK_SPAWN_THRESHOLD)
            continue;
        _tickChunk(owner, level, (int32_t) (column >> 32), (int32_t) (column & 0xffffffff), difficulty, mobCount,
                   players);
    }
}

void NaturalSpawner::_tickChunk(ServerNetworkHandler &owner, Level &level, int32_t chunkX, int32_t chunkZ,
                                int32_t difficulty, int32_t mobCount, const std::vector<Vector3f> &players) {
    if (!level.isChunkResident(chunkX, chunkZ))
        return;

    const int32_t x = chunkX * 16 + _nextInt(16);
    const int32_t z = chunkZ * 16 + _nextInt(16);

    // The column is walked down from the top: its first floor is the surface and every floor below it is
    // underground. Each floor spawns one herd after recounting the mobs around the chunk (sub_1432C2EF0).
    bool surface = true;
    int32_t y = level.getMaxY();
    while (y > level.getMinY()) {
        while (y > level.getMinY() && isSolid(level.getBlockState(x, y, z)))
            --y;
        while (y > level.getMinY() && !isSolid(level.getBlockState(x, y - 1, z)))
            --y;
        if (y <= level.getMinY())
            return;

        SpawnSite site;
        if (_makeSite(level, Vector3i(x, y, z), players, surface, site)) {
            Counts counts;
            _count(owner, level, chunkX, chunkZ, counts);
            _spawnCluster(owner, level, site, difficulty, mobCount, counts);
        }

        surface = false;
        --y;
    }
}

void NaturalSpawner::_count(ServerNetworkHandler &owner, Level &level, int32_t chunkX, int32_t chunkZ,
                            Counts &counts) {
    std::unordered_set<int64_t> alive;
    for (auto &entry: owner.getActors()) {
        const ServerActor &actor = *entry.second;
        if (!actor.isAlive())
            continue;
        alive.insert(entry.first);

        if (!actor.isIn(level))
            continue;

        const Vector3f &position = actor.getPosition();
        const int32_t actorChunkX = (int32_t) std::floor(position.x) >> 4;
        const int32_t actorChunkZ = (int32_t) std::floor(position.z) >> 4;
        if (std::abs(actorChunkX - chunkX) > COUNT_CHUNK_RADIUS || std::abs(actorChunkZ - chunkZ) > COUNT_CHUNK_RADIUS)
            continue;

        // A mob that was not spawned on the surface counts as underground, as a missing "Surface" tag does.
        const int32_t row = mSurfaceMobs.count(entry.first) != 0 ? 1 : 0;
        const int32_t pool = poolIndex(SpawnRules::getPopulation(actor.getIdentifier()));
        if (pool >= 0)
            ++counts.mPools[row][pool];
        ++counts.mTypes[row][actor.getIdentifier()];
    }

    for (auto iterator = mSurfaceMobs.begin(); iterator != mSurfaceMobs.end();) {
        if (alive.count(*iterator) == 0)
            iterator = mSurfaceMobs.erase(iterator);
        else
            ++iterator;
    }
}

void NaturalSpawner::_spawnCluster(ServerNetworkHandler &owner, Level &level, const SpawnSite &site,
                                   int32_t difficulty, int32_t mobCount, Counts &counts) {
    struct Candidate {
        const SpawnRule *mRule;
        const SpawnCondition *mCondition;
    };

    std::vector<Candidate> candidates;
    int32_t totalWeight = 0;
    for (const SpawnRule &rule: SpawnRules::getRules()) {
        const MobActor *prototype = ActorClassRegistry::getMobPrototype(rule.mIdentifier);
        if (prototype == nullptr
            || !prototype->canSpawnNaturally(level, site.mPosition, site.mBiome, site.mLight, mRandom))
            continue;

        for (const SpawnCondition &condition: rule.mConditions) {
            if (!_matches(level, site, condition, difficulty))
                continue;

            candidates.push_back(Candidate{&rule, &condition});
            totalWeight += condition.mWeight;
        }
    }
    if (totalWeight <= 0)
        return;

    int32_t roll = _nextInt(totalWeight);
    const Candidate *chosen = nullptr;
    for (const Candidate &candidate: candidates) {
        roll -= candidate.mCondition->mWeight;
        if (roll < 0) {
            chosen = &candidate;
            break;
        }
    }
    if (chosen == nullptr)
        return;

    const SpawnRule &rule = *chosen->mRule;
    const SpawnCondition &condition = *chosen->mCondition;
    if (condition.mRarity > 0 && _nextInt(condition.mRarity) != 0)
        return;

    // Squaring the roll biases herds towards their minimum size (0x1432C1E8F).
    const SpawnHerd &herd = condition.mHerds[(size_t) _nextInt((int32_t) condition.mHerds.size())];
    const float factor = _nextFloat();
    int32_t size = herd.mMinSize + (int32_t) std::round((float) (herd.mMaxSize - herd.mMinSize) * (factor * factor));
    if (size <= 0 || !_canSpawnAt(level, site, condition))
        return;

    if (size + mobCount > MAX_SPAWNED_MOBS)
        size = std::max(0, MAX_SPAWNED_MOBS - mobCount);

    const int32_t row = site.mSurface ? 1 : 0;
    const int32_t density = site.mSurface ? condition.mSurfaceDensity : condition.mUndergroundDensity;
    if (density >= 0) {
        const int32_t existing = counts.mTypes[row][rule.mIdentifier];
        if (existing + size >= density)
            size = density - existing;
    }

    // The whole herd shares one position; members only stop at the pool cap (0x1432C21E2, 0x1432C27DD).
    const int32_t pool = poolIndex(rule.mPopulation);
    const Vector3f position((float) site.mPosition.x + HERD_OFFSET, (float) site.mPosition.y,
                            (float) site.mPosition.z + HERD_OFFSET);
    for (; size > 0; --size) {
        if (pool < 0 || POOL_CAPS[row][pool] <= 0 || counts.mPools[row][pool] >= POOL_CAPS[row][pool])
            continue;

        const std::string identifier = permute(condition, rule.mIdentifier, _nextInt(permutationWeight(condition)));
        ServerActor *spawned = owner.spawnActor(level, identifier, position, [](ServerActor &actor) {
            actor.setPersistent(false);
        });
        if (spawned == nullptr)
            continue;

        const int32_t spawnedPool = poolIndex(SpawnRules::getPopulation(identifier));
        if (spawnedPool >= 0)
            ++counts.mPools[row][spawnedPool];
        ++counts.mTypes[row][identifier];
        if (site.mSurface)
            mSurfaceMobs.insert(spawned->getUniqueId());
    }
}

void NaturalSpawner::_despawn(ServerNetworkHandler &owner, Level &level, const std::vector<Vector3f> &players) {
    std::vector<int64_t> removed;

    for (auto &entry: owner.getActors()) {
        ServerActor &actor = *entry.second;
        if (!actor.isAlive() || actor.isPersistent() || !actor.isIn(level))
            continue;

        const DespawnRule *rule = SpawnRules::getDespawnRule(actor.getIdentifier());
        if (rule == nullptr || !rule->mFromDistance)
            continue;

        float nearest = std::numeric_limits<float>::max();
        for (const Vector3f &player: players) {
            const Vector3f position = actor.getPosition();
            const float dx = position.x - player.x;
            const float dy = position.y - player.y;
            const float dz = position.z - player.z;
            nearest = std::min(nearest, dx * dx + dy * dy + dz * dz);
        }

        const float distance = std::sqrt(nearest);
        if (distance > rule->mMaxDistance) {
            removed.push_back(entry.first);
            continue;
        }

        if (distance <= rule->mMinDistance) {
            actor.setFarFromPlayerTicks(0);
            continue;
        }

        actor.setFarFromPlayerTicks(actor.getFarFromPlayerTicks() + 1);
        if (!rule->mFromChance && !rule->mFromInactivity)
            continue;
        if (rule->mFromInactivity && actor.getFarFromPlayerTicks() <= rule->mInactivitySeconds * TICKS_PER_SECOND)
            continue;
        if (rule->mFromChance && _nextInt(rule->mRandomChance) != 0)
            continue;

        removed.push_back(entry.first);
    }

    for (const int64_t uniqueId: removed)
        owner.removeActor(uniqueId);
}

bool NaturalSpawner::_makeSite(Level &level, const Vector3i &position, const std::vector<Vector3f> &players,
                               bool surface, SpawnSite &site) {
    if (position.y <= level.getMinY() || position.y >= level.getMaxY())
        return false;

    const BlockState state = level.getBlockState(position.x, position.y, position.z);
    const BlockState above = level.getBlockState(position.x, position.y + 1, position.z);
    const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);
    const LiquidView liquid(state);
    const LiquidView aboveLiquid(above);

    site.mPosition = position;
    site.mSurface = surface;
    site.mInWater = liquid.isWater();
    site.mInLava = liquid.isLava();
    site.mWater = site.mInWater && aboveLiquid.isWater();
    site.mLava = site.mInLava && aboveLiquid.isLava();
    site.mBubble = liquid.isBubbleColumn();
    site.mBelow = below.mName;
    site.mSnow = state.mName == "minecraft:snow_layer" || below.mName == "minecraft:snow";

    if (!site.mInWater && !site.mInLava && !site.mBubble && (!isPassable(state) || !isPassable(above)))
        return false;

    LevelChunk *chunk = level.peekChunkPtr(position.x >> 4, position.z >> 4);
    site.mBiome = chunk == nullptr ? 0 : (int32_t) chunk->getBiomeAt(position.x & 15, position.y, position.z & 15);

    const int32_t blockLight = level.getBlockLightAt(position.x, position.y, position.z);
    const int32_t skyLight = level.getSkyLightAt(position.x, position.y, position.z);
    site.mLight = std::max(blockLight, skyLight - level.getSkyLightSubtracted());
    site.mLightWithoutWeather = std::max(blockLight,
                                         skyLight - LightSystem::calculateSkyLightSubtracted(level, false));

    float nearest = std::numeric_limits<float>::max();
    for (const Vector3f &player: players) {
        const float dx = (float) position.x - player.x;
        const float dy = (float) position.y - player.y;
        const float dz = (float) position.z - player.z;
        nearest = std::min(nearest, dx * dx + dy * dy + dz * dz);
    }
    site.mPlayerDistance = std::sqrt(nearest);
    return true;
}

bool NaturalSpawner::_matches(Level &level, const SpawnSite &site, const SpawnCondition &condition,
                              int32_t difficulty) const {
    if (!condition.mSupported || condition.mWeight <= 0)
        return false;

    if (condition.mHasHeight && (site.mPosition.y < condition.mMinHeight || site.mPosition.y > condition.mMaxHeight))
        return false;

    // Every environment flag of the site has to be allowed by the rule (sub_145D43A40).
    if (site.mWater && !condition.mUnderwater)
        return false;
    if (site.mLava && !condition.mLava)
        return false;
    if (site.mSurface ? !condition.mOnSurface : !condition.mUnderground)
        return false;
    if (condition.mNoBubbles && site.mBubble)
        return false;

    if (difficulty < condition.mMinDifficulty || difficulty > condition.mMaxDifficulty)
        return false;

    if (condition.mHasBrightness) {
        const int32_t light = condition.mAdjustForWeather ? site.mLight : site.mLightWithoutWeather;
        if (light < condition.mMinBrightness || light > condition.mMaxBrightness)
            return false;
    }

    if (condition.mMinWorldAge > 0 && level.getTime() < condition.mMinWorldAge)
        return false;

    return condition.mBiomeFilter == nullptr || testFilter(*condition.mBiomeFilter, site.mBiome, site.mSnow);
}

bool NaturalSpawner::_canSpawnAt(Level &level, const SpawnSite &site, const SpawnCondition &condition) const {
    // The spawn position needs its neighbouring chunks and a player within the rule's distances (sub_145E16490).
    const int32_t chunkX = site.mPosition.x >> 4;
    const int32_t chunkZ = site.mPosition.z >> 4;
    for (int32_t neighbourX = chunkX - 1; neighbourX <= chunkX + 1; ++neighbourX) {
        for (int32_t neighbourZ = chunkZ - 1; neighbourZ <= chunkZ + 1; ++neighbourZ) {
            if (!level.isChunkResident(neighbourX, neighbourZ))
                return false;
        }
    }

    if (site.mPlayerDistance > (float) condition.mMaxDistance || site.mPlayerDistance < (float) condition.mMinDistance)
        return false;

    if (!condition.mOnBlocks.empty()
        && std::find(condition.mOnBlocks.begin(), condition.mOnBlocks.end(), site.mBelow) == condition.mOnBlocks.end())
        return false;

    if (std::find(condition.mPreventedBlocks.begin(), condition.mPreventedBlocks.end(), site.mBelow)
        != condition.mPreventedBlocks.end())
        return false;

    return condition.mUnderwater == site.mInWater && condition.mLava == site.mInLava;
}
