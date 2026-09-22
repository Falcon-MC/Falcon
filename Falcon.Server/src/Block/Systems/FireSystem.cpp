#include "Block/Systems/FireSystem.h"

#include "Actor/MobEffect.h"
#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Block/BlockData.h"
#include "Block/BlockState.h"
#include "Block/BlockSupport.h"
#include "Block/Blocks/FireBlock.h"
#include "Block/Blocks/PlacementRuleBlocks.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Systems/RedstoneSystem.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Block/Blocks/PortalBlocks.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
    struct ScheduledPosition {
        int32_t x;
        int32_t y;
        int32_t z;
    };

    struct FireSchedule {
        int64_t mTick = 0;
        std::unordered_map<int64_t, int64_t> mScheduled;
        std::map<int64_t, std::vector<ScheduledPosition>> mBuckets;
    };

    std::array<FireSchedule, Dimension::DIMENSION_COUNT> gSchedules;

    FireSchedule &scheduleOf(Level &level) {
        return gSchedules[level.getDimensionId()];
    }

    const char *UNBURNABLE_BLOCKS[] = {
            "minecraft:crimson_button",
            "minecraft:crimson_door",
            "minecraft:crimson_double_slab",
            "minecraft:crimson_fence",
            "minecraft:crimson_fence_gate",
            "minecraft:crimson_planks",
            "minecraft:crimson_pressure_plate",
            "minecraft:crimson_slab",
            "minecraft:crimson_stairs",
            "minecraft:crimson_trapdoor",
            "minecraft:crimson_wall_sign",
            "minecraft:warped_button",
            "minecraft:warped_door",
            "minecraft:warped_double_slab",
            "minecraft:warped_fence",
            "minecraft:warped_fence_gate",
            "minecraft:warped_planks",
            "minecraft:warped_pressure_plate",
            "minecraft:warped_slab",
            "minecraft:warped_stairs",
            "minecraft:warped_trapdoor",
            "minecraft:ice"
    };

    const int NEIGHBOUR_BOUND_HORIZONTAL = 300;
    const int NEIGHBOUR_BOUND_VERTICAL = 250;
    const int SPREAD_BASE_CHANCE = 100;
    const int SPREAD_HEIGHT_PENALTY = 100;
    const int SPREAD_CHANCE_BONUS = 40;
    const int SPREAD_DIFFICULTY_FACTOR = 7;
    const int SPREAD_AGE_OFFSET = 30;
    const int MAX_EXTRA_DELAY = 10;
    const int SUPPORTED_FADE_AGE = 3;

    std::mt19937 &fireRandom() {
        static std::mt19937 generator{std::random_device{}()};
        return generator;
    }

    int nextInt(int bound) {
        if (bound <= 1)
            return 0;

        std::uniform_int_distribution<int> distribution(0, bound - 1);
        return distribution(fireRandom());
    }

    bool endsWith(const std::string &value, const std::string &suffix) {
        if (value.size() < suffix.size())
            return false;

        return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    bool isChunkReady(Level &level, const Vector3i &position) {
        if (position.y < level.getMinY() || position.y > level.getMaxY())
            return false;

        return level.isChunkResident(position.x >> 4, position.z >> 4);
    }

    BlockState stateAt(Level &level, const Vector3i &position) {
        if (!isChunkReady(level, position))
            return BlockState("minecraft:air");

        return level.getBlockState(position.x, position.y, position.z);
    }

    int stateInt(const BlockState &state, const std::string &key, int fallback) {
        const Tag *tag = state.mStates.get(key);
        if (tag == nullptr || tag->getType() != Tag::Type::Int)
            return fallback;

        return tag->asInt();
    }

    Vector3i relative(const Vector3i &position, int dx, int dy, int dz) {
        return Vector3i(position.x + dx, position.y + dy, position.z + dz);
    }

    bool isTopFacingSurfaceSolid(const BlockState &state) {
        const std::string &identifier = state.mName;

        if (endsWith(identifier, "_stairs"))
            return false;

        if (VanillaBlocks::getAs<SlabBlock>(identifier) != nullptr)
            return SlabBlock::isTopSlab(state);

        if (identifier == "minecraft:snow_layer")
            return false;

        if (endsWith(identifier, "_fence_gate") || endsWith(identifier, "trapdoor"))
            return false;

        if (identifier == "minecraft:moss_carpet" || identifier == "minecraft:azalea")
            return false;

        return BlockSupport::isSolid(state);
    }

    bool burnsForever(const std::string &identifier) {
        return identifier == "minecraft:netherrack" || identifier == "minecraft:magma";
    }

    bool seesSky(Level &level, const Vector3i &position) {
        return level.getHeightAt(position.x, position.z) <= position.y;
    }

    bool canNeighbourBurn(Level &level, const Vector3i &position) {
        static const int OFFSETS[6][3] = {
                {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}
        };

        for (const auto &offset: OFFSETS) {
            const Vector3i side = relative(position, offset[0], offset[1], offset[2]);
            if (FireSystem::getBurnChance(stateAt(level, side).mName) > 0)
                return true;
        }

        return false;
    }

    bool isFireTickEnabled(ServerNetworkHandler &owner) {
        return owner.getLevel().getGameRules().getBool("dofiretick");
    }

    void extinguish(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
        const BlockState previous = stateAt(level, position);
        level.setBlock(position, BlockState("minecraft:air"), false);
        level.onBlockBroken(position, previous);
    }

    void setFire(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const std::string &identifier,
                 int age) {
        Tag states = Tag::ofCompound();
        states.putInt("age", std::clamp(age, 0, FireSystem::MAX_AGE));

        const BlockState placed(identifier, states);
        level.setBlock(position, placed, false);
        level.onBlockPlaced(position, placed);
    }

    bool checkRain(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
        if (!level.hasSkyLight() || !level.isRaining())
            return false;

        if (burnsForever(stateAt(level, relative(position, 0, -1, 0)).mName))
            return false;

        if (!seesSky(level, position) && !seesSky(level, relative(position, 1, 0, 0))
            && !seesSky(level, relative(position, -1, 0, 0)) && !seesSky(level, relative(position, 0, 0, 1))
            && !seesSky(level, relative(position, 0, 0, -1)))
            return false;

        extinguish(owner, level, position);
        return true;
    }

    int chanceOfNeighboursEncouragingFire(Level &level, const Vector3i &position) {
        if (stateAt(level, position).mName != "minecraft:air")
            return 0;

        int chance = 0;
        chance = std::max(chance, FireSystem::getBurnChance(stateAt(level, relative(position, 1, 0, 0)).mName));
        chance = std::max(chance, FireSystem::getBurnChance(stateAt(level, relative(position, -1, 0, 0)).mName));
        chance = std::max(chance, FireSystem::getBurnChance(stateAt(level, relative(position, 0, -1, 0)).mName));
        chance = std::max(chance, FireSystem::getBurnChance(stateAt(level, relative(position, 0, 1, 0)).mName));
        chance = std::max(chance, FireSystem::getBurnChance(stateAt(level, relative(position, 0, 0, 1)).mName));
        chance = std::max(chance, FireSystem::getBurnChance(stateAt(level, relative(position, 0, 0, -1)).mName));
        return chance;
    }

    void tryToCatchBlockOnFire(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int bound,
                               int age) {
        const BlockState state = stateAt(level, position);
        const int burnAbility = FireSystem::getBurnAbility(state.mName);

        if (nextInt(bound) >= burnAbility)
            return;

        if (nextInt(age + 10) < 5) {
            setFire(owner, level, position, "minecraft:fire", std::min(age + nextInt(5) / 4, FireSystem::MAX_AGE));
            FireSystem::scheduleUpdate(level, position, FireSystem::TICK_RATE);
            return;
        }

        extinguish(owner, level, position);
    }

    void touchActor(ServerNetworkHandler &owner, Level &level, ServerActor &actor) {
        if (actor.isDead() || actor.isProjectile() || actor.getDimension() != level.getDimensionType())
            return;

        if (actor.hasEffect(MobEffectId::FireResistance))
            return;

        const Vector3f position = actor.getPosition();
        const Vector3i block((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                             (int32_t) std::floor(position.z));

        if (!FireSystem::matches(stateAt(level, block).mName))
            return;

        actor.hurt(owner, FireSystem::CONTACT_DAMAGE, nullptr);

        if (actor.getFireTicks() < FireSystem::COMBUST_TICKS) {
            actor.setFireTicks(FireSystem::COMBUST_TICKS);
            owner.syncActorFlags(actor);
        }
    }

    void touchPlayer(ServerNetworkHandler &owner, Level &level, ServerPlayer &player) {
        if (!player.isSpawned() || player.isDead() || player.getDimension() != level.getDimensionType())
            return;

        const int32_t gameType = player.getGameType();
        if (gameType == (int32_t) GameType::Creative || gameType == (int32_t) GameType::Spectator)
            return;

        if (player.hasEffect(MobEffectId::FireResistance))
            return;

        const Vector3f position = player.getPosition();
        const Vector3i block((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                             (int32_t) std::floor(position.z));

        if (!FireSystem::matches(stateAt(level, block).mName))
            return;

        owner.applyDamage(player, FireSystem::CONTACT_DAMAGE, "death.attack.inFire", {player.getName()});

        if (player.getFireTicks() < FireSystem::COMBUST_TICKS)
            player.setFireTicks(FireSystem::COMBUST_TICKS);
    }
}

bool FireSystem::matches(const std::string &identifier) {
    return VanillaBlocks::getAs<FireBlock>(identifier) != nullptr;
}

int FireSystem::getBurnChance(const std::string &identifier) {
    if (!canBeIgnitedAgainst(identifier))
        return UNBURNABLE;

    const BlockData *data = BlockDataTable::find(identifier.c_str());
    return data == nullptr ? 0 : (int) data->mBurnChance;
}

int FireSystem::getBurnAbility(const std::string &identifier) {
    const BlockData *data = BlockDataTable::find(identifier.c_str());
    return data == nullptr ? 0 : (int) data->mBurnAbility;
}

bool FireSystem::canBeIgnitedAgainst(const std::string &identifier) {
    for (const char *unburnable: UNBURNABLE_BLOCKS) {
        if (identifier == unburnable)
            return false;
    }

    return true;
}

bool FireSystem::canSurviveAt(Level &level, const Vector3i &position) {
    return isTopFacingSurfaceSolid(stateAt(level, relative(position, 0, -1, 0)))
           || canNeighbourBurn(level, position);
}

bool FireSystem::ignite(ServerNetworkHandler &owner, Level &level, const Vector3i &position, bool canLightPortal) {
    if (!isChunkReady(level, position))
        return false;

    if (stateAt(level, position).mName != "minecraft:air")
        return false;

    if (canLightPortal && ObsidianBlock::tryLightPortal(level, position, &owner))
        return true;

    if (!canSurviveAt(level, position))
        return false;

    const std::string below = stateAt(level, relative(position, 0, -1, 0)).mName;
    const bool soul = below == "minecraft:soul_sand" || below == "minecraft:soul_soil";

    setFire(owner, level, position, soul ? "minecraft:soul_fire" : "minecraft:fire", 0);
    scheduleUpdate(level, position, TICK_RATE + nextInt(MAX_EXTRA_DELAY));
    return true;
}

void FireSystem::onNormalUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                const BlockState &state) {
    if (!matches(state.mName))
        return;

    const std::string below = stateAt(level, relative(position, 0, -1, 0)).mName;

    if (state.mName == "minecraft:fire" && (below == "minecraft:soul_sand" || below == "minecraft:soul_soil")) {
        setFire(owner, level, position, "minecraft:soul_fire", stateInt(state, "age", 0));
        return;
    }

    if (!canSurviveAt(level, position)) {
        extinguish(owner, level, position);
        return;
    }

    if (isFireTickEnabled(owner) &&
        scheduleOf(level).mScheduled.count(RedstoneSystem::packPosition(position)) == 0)
        scheduleUpdate(level, position, TICK_RATE);

    checkRain(owner, level, position);
}

void FireSystem::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                   const BlockState &state) {
    if (!matches(state.mName) || !isFireTickEnabled(owner))
        return;

    if (!canSurviveAt(level, position)) {
        extinguish(owner, level, position);
        return;
    }

    if (checkRain(owner, level, position))
        return;

    const Vector3i below = relative(position, 0, -1, 0);
    const BlockState belowState = stateAt(level, below);
    const bool forever = state.mName == "minecraft:soul_fire" || burnsForever(belowState.mName);
    const int age = stateInt(state, "age", 0);

    if (age < MAX_AGE)
        setFire(owner, level, position, state.mName, std::min(age + nextInt(3), MAX_AGE));

    scheduleUpdate(level, position, TICK_RATE + nextInt(MAX_EXTRA_DELAY));

    if (!forever && !canNeighbourBurn(level, position)) {
        if (!isTopFacingSurfaceSolid(belowState) || age > SUPPORTED_FADE_AGE)
            extinguish(owner, level, position);
        return;
    }

    if (!forever && getBurnAbility(belowState.mName) == 0 && age == MAX_AGE && nextInt(4) == 0) {
        extinguish(owner, level, position);
        return;
    }

    tryToCatchBlockOnFire(owner, level, relative(position, 1, 0, 0), NEIGHBOUR_BOUND_HORIZONTAL, age);
    tryToCatchBlockOnFire(owner, level, relative(position, -1, 0, 0), NEIGHBOUR_BOUND_HORIZONTAL, age);
    tryToCatchBlockOnFire(owner, level, below, NEIGHBOUR_BOUND_VERTICAL, age);
    tryToCatchBlockOnFire(owner, level, relative(position, 0, 1, 0), NEIGHBOUR_BOUND_VERTICAL, age);
    tryToCatchBlockOnFire(owner, level, relative(position, 0, 0, 1), NEIGHBOUR_BOUND_HORIZONTAL, age);
    tryToCatchBlockOnFire(owner, level, relative(position, 0, 0, -1), NEIGHBOUR_BOUND_HORIZONTAL, age);

    const int difficulty = (int) owner.getProperties().getDifficulty();

    for (int32_t x = position.x - 1; x <= position.x + 1; ++x) {
        for (int32_t z = position.z - 1; z <= position.z + 1; ++z) {
            for (int32_t y = position.y - 1; y <= position.y + 4; ++y) {
                if (x == position.x && y == position.y && z == position.z)
                    continue;

                int bound = SPREAD_BASE_CHANCE;
                if (y > position.y + 1)
                    bound += (y - (position.y + 1)) * SPREAD_HEIGHT_PENALTY;

                const Vector3i target(x, y, z);
                const int chance = chanceOfNeighboursEncouragingFire(level, target);
                if (chance <= 0)
                    continue;

                const int threshold = (chance + SPREAD_CHANCE_BONUS + difficulty * SPREAD_DIFFICULTY_FACTOR)
                                      / (age + SPREAD_AGE_OFFSET);
                if (threshold <= 0 || nextInt(bound) > threshold)
                    continue;

                setFire(owner, level, target, "minecraft:fire", std::min(age + nextInt(5) / 4, MAX_AGE));
                scheduleUpdate(level, target, TICK_RATE);
            }
        }
    }
}

void FireSystem::scheduleUpdate(Level &level, const Vector3i &position, int64_t delay) {
    if (delay < 1)
        delay = 1;

    FireSchedule &schedule = scheduleOf(level);
    const int64_t key = RedstoneSystem::packPosition(position);
    const int64_t target = schedule.mTick + delay;

    const auto it = schedule.mScheduled.find(key);
    if (it != schedule.mScheduled.end() && it->second <= target)
        return;

    schedule.mScheduled[key] = target;

    ScheduledPosition entry;
    entry.x = position.x;
    entry.y = position.y;
    entry.z = position.z;
    schedule.mBuckets[target].push_back(entry);
}

void FireSystem::tick(ServerNetworkHandler &owner, Level &level) {
    FireSchedule &schedule = scheduleOf(level);
    ++schedule.mTick;

    std::vector<ScheduledPosition> due;

    while (!schedule.mBuckets.empty()) {
        const auto it = schedule.mBuckets.begin();
        if (it->first > schedule.mTick)
            break;

        for (const ScheduledPosition &entry: it->second)
            due.push_back(entry);

        schedule.mBuckets.erase(it);
    }

    for (const ScheduledPosition &entry: due) {
        const Vector3i position(entry.x, entry.y, entry.z);
        const int64_t key = RedstoneSystem::packPosition(position);

        const auto scheduledIt = schedule.mScheduled.find(key);
        if (scheduledIt == schedule.mScheduled.end() || scheduledIt->second > schedule.mTick)
            continue;

        schedule.mScheduled.erase(scheduledIt);
        onScheduledUpdate(owner, level, position, stateAt(level, position));
    }

    for (auto &entry: owner.getPlayers())
        touchPlayer(owner, level, entry.second);

    for (auto &entry: owner.getActors()) {
        ServerActor *actor = entry.second.get();
        if (actor != nullptr)
            touchActor(owner, level, *actor);
    }
}
