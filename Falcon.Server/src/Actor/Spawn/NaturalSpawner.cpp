#include "Actor/Spawn/NaturalSpawner.h"

#include "Actor/ActorClassRegistry.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Block/BlockData.h"
#include "Block/Blocks/LiquidView.h"
#include "Level/Generator/Biome/BiomeChunkGenDataRegistry.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Level/LightSystem.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
    const NaturalSpawner::Category CATEGORIES[] = {
            {"monster",      70, 1,   true},
            {"animal",       10, 400, false},
            {"water_animal", 5,  1,   false},
            {"ambient",      15, 1,   false}
    };

    const float SPAWN_MIN_DISTANCE = 24.0f;
    const float SPAWN_MAX_DISTANCE = 44.0f;
    const float COUNT_RADIUS = 128.0f;
    const int32_t HERD_SPREAD = 4;
    const int32_t UNDERGROUND_SEARCH = 16;
    const int32_t TICKS_PER_SECOND = 20;

    bool isSolid(const BlockState &state) {
        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        return data != nullptr && data->mSolid;
    }

    bool isPassable(const BlockState &state) {
        const LiquidView liquid(state);
        return !isSolid(state) && !liquid.isLiquid() && !liquid.isBubbleColumn();
    }

    float horizontalDistanceSquared(const Vector3f &left, const Vector3f &right) {
        const float dx = left.x - right.x;
        const float dz = left.z - right.z;
        return dx * dx + dz * dz;
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

void NaturalSpawner::tick(ServerNetworkHandler &owner, Level &level) {
    std::vector<Vector3f> players;
    for (auto &entry: owner.getPlayers()) {
        const ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || player.getDimension() != level.getDimensionType()
            || player.getGameType() == (int32_t) GameType::Spectator)
            continue;
        players.push_back(player.getPosition());
    }
    if (players.empty())
        return;

    _despawn(owner, level, players);

    if (!level.getGameRules().getBool("domobspawning"))
        return;

    std::vector<NearbyActor> nearby;
    for (auto &entry: owner.getActors()) {
        const ServerActor &actor = *entry.second;
        if (!actor.isAlive() || actor.getDimension() != level.getDimensionType())
            continue;

        const std::string &population = SpawnRules::getPopulation(actor.getIdentifier());
        if (!population.empty())
            nearby.push_back(NearbyActor{actor.getPosition(), &population, actor.getIdentifier()});
    }

    const int32_t difficulty = (int32_t) owner.getProperties().getDifficulty();
    const int64_t currentTick = owner.getCurrentTick();
    const float countRadiusSquared = COUNT_RADIUS * COUNT_RADIUS;

    for (const Vector3f &player: players) {
        for (const Category &category: CATEGORIES) {
            if (currentTick % category.mInterval != 0 || (category.mHostile && difficulty == 0))
                continue;

            int32_t count = 0;
            for (const NearbyActor &actor: nearby) {
                if (*actor.mPopulation == category.mName
                    && horizontalDistanceSquared(actor.mPosition, player) <= countRadiusSquared)
                    ++count;
            }

            if (count >= category.mCap)
                continue;

            const int32_t minChunkX = (int32_t) std::floor((player.x - SPAWN_MAX_DISTANCE) / 16.0f);
            const int32_t maxChunkX = (int32_t) std::floor((player.x + SPAWN_MAX_DISTANCE) / 16.0f);
            const int32_t minChunkZ = (int32_t) std::floor((player.z - SPAWN_MAX_DISTANCE) / 16.0f);
            const int32_t maxChunkZ = (int32_t) std::floor((player.z + SPAWN_MAX_DISTANCE) / 16.0f);
            for (int32_t chunkX = minChunkX; chunkX <= maxChunkX && count < category.mCap; ++chunkX) {
                for (int32_t chunkZ = minChunkZ; chunkZ <= maxChunkZ && count < category.mCap; ++chunkZ)
                    count += _attempt(owner, level, player, chunkX, chunkZ, category, difficulty, players, nearby);
            }
        }
    }
}

void NaturalSpawner::_despawn(ServerNetworkHandler &owner, Level &level, const std::vector<Vector3f> &players) {
    std::vector<int64_t> removed;

    for (auto &entry: owner.getActors()) {
        ServerActor &actor = *entry.second;
        if (!actor.isAlive() || actor.isPersistent() || actor.getDimension() != level.getDimensionType())
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
                               bool water, SpawnSite &site) {
    if (!level.isChunkResident(position.x >> 4, position.z >> 4))
        return false;
    if (position.y <= level.getMinY() || position.y >= level.getMaxY())
        return false;

    const BlockState state = level.getBlockState(position.x, position.y, position.z);
    const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);
    const LiquidView liquid(state);

    site.mPosition = position;
    site.mWater = liquid.isWater();
    site.mLava = liquid.isLava();
    site.mBubble = liquid.isBubbleColumn();
    site.mBelow = below.mName;
    site.mSnow = state.mName == "minecraft:snow_layer" || below.mName == "minecraft:snow";

    if (water && !site.mWater && !site.mBubble)
        return false;

    if (!site.mWater && !site.mLava && !site.mBubble) {
        if (!isPassable(state) || !isPassable(level.getBlockState(position.x, position.y + 1, position.z)))
            return false;
        if (!isSolid(below))
            return false;
    }

    site.mSurface = position.y >= level.getHeightAt(position.x, position.z);

    LevelChunk *chunk = level.peekChunkPtr(position.x >> 4, position.z >> 4);
    site.mBiome = chunk == nullptr ? 0 : (int32_t) chunk->getBiomeAt(position.x & 15, position.y, position.z & 15);

    const int32_t blockLight = level.getBlockLightAt(position.x, position.y, position.z);
    const int32_t skyLight = level.getSkyLightAt(position.x, position.y, position.z);
    site.mLight = std::max(blockLight, skyLight - level.getSkyLightSubtracted());
    site.mLightWithoutWeather = std::max(blockLight,
                                         skyLight - LightSystem::calculateSkyLightSubtracted(level, false));

    float nearest = std::numeric_limits<float>::max();
    for (const Vector3f &player: players) {
        const float dx = (float) position.x + 0.5f - player.x;
        const float dy = (float) position.y - player.y;
        const float dz = (float) position.z + 0.5f - player.z;
        nearest = std::min(nearest, dx * dx + dy * dy + dz * dz);
    }
    site.mPlayerDistance = std::sqrt(nearest);
    return true;
}

bool NaturalSpawner::_matches(Level &level, const SpawnSite &site, const SpawnCondition &condition,
                              int32_t difficulty, const std::string &identifier, const Vector3f &player,
                              const std::vector<NearbyActor> &nearby) const {
    if (!condition.mSupported)
        return false;

    if (site.mWater || site.mBubble) {
        if (!condition.mUnderwater || (condition.mNoBubbles && site.mBubble))
            return false;
    } else if (site.mLava) {
        if (!condition.mLava)
            return false;
    } else if (site.mSurface ? !condition.mOnSurface : !condition.mUnderground) {
        return false;
    }

    if (difficulty < condition.mMinDifficulty || difficulty > condition.mMaxDifficulty)
        return false;

    if (condition.mHasBrightness) {
        const int32_t light = condition.mAdjustForWeather ? site.mLight : site.mLightWithoutWeather;
        if (light < condition.mMinBrightness || light > condition.mMaxBrightness)
            return false;
    }

    if (condition.mHasHeight && (site.mPosition.y < condition.mMinHeight || site.mPosition.y > condition.mMaxHeight))
        return false;

    if (condition.mHasDistance
        && (site.mPlayerDistance < condition.mMinDistance || site.mPlayerDistance > condition.mMaxDistance))
        return false;

    if (condition.mMinWorldAge > 0 && level.getTime() < condition.mMinWorldAge)
        return false;

    if (!condition.mOnBlocks.empty()
        && std::find(condition.mOnBlocks.begin(), condition.mOnBlocks.end(), site.mBelow) == condition.mOnBlocks.end())
        return false;

    if (std::find(condition.mPreventedBlocks.begin(), condition.mPreventedBlocks.end(), site.mBelow)
        != condition.mPreventedBlocks.end())
        return false;

    if (condition.mBiomeFilter != nullptr && !testFilter(*condition.mBiomeFilter, site.mBiome, site.mSnow))
        return false;

    const int32_t density = site.mSurface ? condition.mSurfaceDensity : condition.mUndergroundDensity;
    if (density >= 0) {
        const float radiusSquared = COUNT_RADIUS * COUNT_RADIUS;
        int32_t count = 0;
        for (const NearbyActor &actor: nearby) {
            if (actor.mIdentifier == identifier && horizontalDistanceSquared(actor.mPosition, player) <= radiusSquared)
                ++count;
        }
        if (count >= density)
            return false;
    }

    return true;
}

int32_t NaturalSpawner::_attempt(ServerNetworkHandler &owner, Level &level, const Vector3f &player, int32_t chunkX,
                                 int32_t chunkZ, const Category &category, int32_t difficulty,
                                 const std::vector<Vector3f> &players, const std::vector<NearbyActor> &nearby) {
    if (!level.isChunkResident(chunkX, chunkZ))
        return 0;

    const int32_t x = chunkX * 16 + _nextInt(16);
    const int32_t z = chunkZ * 16 + _nextInt(16);
    const Vector3f column((float) x + 0.5f, player.y, (float) z + 0.5f);
    const float distanceSquared = horizontalDistanceSquared(column, player);
    if (distanceSquared < SPAWN_MIN_DISTANCE * SPAWN_MIN_DISTANCE
        || distanceSquared > SPAWN_MAX_DISTANCE * SPAWN_MAX_DISTANCE)
        return 0;

    for (const Vector3f &other: players) {
        if (horizontalDistanceSquared(column, other) < SPAWN_MIN_DISTANCE * SPAWN_MIN_DISTANCE)
            return 0;
    }

    const bool water = std::string(category.mName) == "water_animal";
    const int32_t height = level.getHeightAt(x, z);
    const int32_t bottom = level.getMinY() + 1;

    int32_t y = height;
    if (water || _nextInt(2) == 0) {
        y = bottom + _nextInt(std::max(1, height - bottom));
        if (!water) {
            const int32_t lowest = std::max(bottom, y - UNDERGROUND_SEARCH);
            while (y > lowest && !isPassable(level.getBlockState(x, y, z)))
                --y;
            while (y > lowest && isPassable(level.getBlockState(x, y - 1, z)))
                --y;
        }
    }

    SpawnSite site;
    if (!_makeSite(level, Vector3i(x, y, z), players, water, site))
        return 0;

    struct Candidate {
        const SpawnRule *mRule;
        const SpawnCondition *mCondition;
    };

    std::vector<Candidate> candidates;
    int32_t totalWeight = 0;
    for (const SpawnRule &rule: SpawnRules::getRules()) {
        const MobActor *prototype = ActorClassRegistry::getMobPrototype(rule.mIdentifier);
        if (rule.mPopulation != category.mName || prototype == nullptr
            || !prototype->canSpawnNaturally(level, site.mPosition, site.mBiome, site.mLight, mRandom))
            continue;

        for (const SpawnCondition &condition: rule.mConditions) {
            if (condition.mWeight <= 0
                || !_matches(level, site, condition, difficulty, rule.mIdentifier, player, nearby))
                continue;

            candidates.push_back(Candidate{&rule, &condition});
            totalWeight += condition.mWeight;
        }
    }
    if (totalWeight <= 0)
        return 0;

    int32_t roll = _nextInt(totalWeight);
    const Candidate *chosen = &candidates.back();
    for (const Candidate &candidate: candidates) {
        roll -= candidate.mCondition->mWeight;
        if (roll < 0) {
            chosen = &candidate;
            break;
        }
    }

    const SpawnCondition &condition = *chosen->mCondition;
    const SpawnHerd &herd = condition.mHerds[(size_t) _nextInt((int32_t) condition.mHerds.size())];
    const int32_t size = herd.mMinSize + _nextInt(herd.mMaxSize - herd.mMinSize + 1);

    int32_t spawned = 0;
    for (int32_t member = 0; member < size; ++member) {
        SpawnSite memberSite = site;
        if (member > 0) {
            Vector3i position = site.mPosition;
            position.x += _nextInt(HERD_SPREAD * 2 + 1) - HERD_SPREAD;
            position.z += _nextInt(HERD_SPREAD * 2 + 1) - HERD_SPREAD;
            if (!level.isChunkResident(position.x >> 4, position.z >> 4))
                continue;
            if (site.mSurface)
                position.y = level.getHeightAt(position.x, position.z);

            if (!_makeSite(level, position, players, water, memberSite)
                || !_matches(level, memberSite, condition, difficulty, chosen->mRule->mIdentifier, player, nearby))
                continue;
        }

        const std::string identifier = permute(condition, chosen->mRule->mIdentifier,
                                                _nextInt(permutationWeight(condition)));
        const Vector3f position((float) memberSite.mPosition.x + 0.5f, (float) memberSite.mPosition.y,
                                (float) memberSite.mPosition.z + 0.5f);
        if (owner.spawnActor(level, identifier, position, [](ServerActor &actor) {
            actor.setPersistent(false);
        }) != nullptr)
            ++spawned;
    }
    return spawned;
}
