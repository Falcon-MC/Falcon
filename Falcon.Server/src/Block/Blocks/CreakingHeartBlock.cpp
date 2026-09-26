#include "Block/Blocks/CreakingHeartBlock.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Level.h"
#include "Level/Particle/CreakingHeartTrailParticle.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <initializer_list>
#include <random>

FALCON_REGISTER_BLOCK(CreakingHeartBlock, 185);

namespace {
    const char *const HEART_STATE = "creaking_heart_state";
    const char *const UPROOTED = "uprooted";
    const char *const DORMANT = "dormant";
    const char *const AWAKE = "awake";
    const char *const NATURAL_STATE = "natural";
    const char *const PILLAR_AXIS_STATE = "pillar_axis";
    const char *const DEFAULT_AXIS = "y";
    const char *const MULTI_FACE_STATE = "multi_face_direction_bits";
    const char *const AIR = "minecraft:air";
    const char *const PALE_OAK_LOG = "minecraft:pale_oak_log";
    const char *const CREAKING = "minecraft:creaking";
    const char *const INSTANT_DESPAWN_COMPONENT = "minecraft:instant_despawn";
    const char *const SPAWNED_BY_HEART_EVENT = "minecraft:entity_spawned_by_creaking_heart";
    const char *const START_TWITCHING_EVENT = "minecraft:start_twitching";
    const char *const DAMAGED_BY_PLAYER_EVENT = "minecraft:on_spawned_creaking_damaged_by_player";
    const char *const CRUMBLING_EVENT = "minecraft:on_spawned_creaking_crumbling";
    const char *const SPAWN_SOUND = "creaking_heart_spawn";
    const char *const TRAIL_SOUND = "block.creaking_heart.trail";

    const int32_t UPDATE_TICKS = 20;
    const int32_t UPDATE_TICKS_VARIANCE = 5;
    const float PLAYER_RANGE = 32.0f;
    const int32_t SPAWN_ATTEMPTS = 5;
    const double SPAWN_RANGE_HORIZONTAL = 16.0;
    const int32_t SPAWN_RANGE_VERTICAL = 8;
    const int32_t RESIN_SEARCH_RADIUS = 2;
    const int32_t RESIN_MIN_COUNT = 1;
    const int32_t RESIN_EXTRA_COUNT = 2;

    struct ResinFace {
        int32_t mX;
        int32_t mY;
        int32_t mZ;
        int32_t mBit;
    };

    const ResinFace RESIN_FACES[] = {
            {0, -1, 0, 1 << 1},
            {0, 1, 0, 1 << 0},
            {0, 0, -1, 1 << 2},
            {0, 0, 1, 1 << 4},
            {-1, 0, 0, 1 << 5},
            {1, 0, 0, 1 << 3}
    };

    std::mt19937 &heartRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    bool isAir(const BlockState *state) {
        return state != nullptr && state->mName == AIR;
    }

    bool isNaturalNight(Level &level) {
        return level.getDimensionType() == DimensionType::Overworld && level.isNight();
    }
}

bool CreakingHeartBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:creaking_heart";
}

bool CreakingHeartBlock::bindsHomeActors() const {
    return true;
}

std::vector<MobActor *> CreakingHeartBlock::findBoundActors(ServerNetworkHandler &owner, Level &level,
                                                            const Vector3i &position) {
    std::vector<MobActor *> bound;
    for (auto &entry: owner.getActors()) {
        MobActor *mob = dynamic_cast<MobActor *>(entry.second.get());
        if (mob == nullptr || !mob->hasHome() || mob->isExpired() || !mob->isAlive()
            || mob->getComponent(INSTANT_DESPAWN_COMPONENT) != nullptr || &owner.getLevelFor(*mob) != &level)
            continue;

        const Vector3f &home = mob->getHomePosition();
        if ((int32_t) std::floor(home.x) == position.x && (int32_t) std::floor(home.y) == position.y
            && (int32_t) std::floor(home.z) == position.z)
            bound.push_back(mob);
    }
    return bound;
}

void CreakingHeartBlock::scheduleNextUpdate(Level &level, const Vector3i &position) {
    if (level.getDimensionType() != DimensionType::Overworld || level.isUpdateScheduled(position))
        return;

    level.scheduleUpdate(position, UPDATE_TICKS + RandomTickSystem::nextInt(UPDATE_TICKS_VARIANCE + 1));
}

bool CreakingHeartBlock::hasRequiredLogs(Level &level, const Vector3i &position, const std::string &axis) {
    const int32_t dx = axis == "x" ? 1 : 0;
    const int32_t dy = axis == "y" ? 1 : 0;
    const int32_t dz = axis == "z" ? 1 : 0;

    for (const int32_t sign: {-1, 1}) {
        const BlockState *neighbour = level.peekBlockPtr(position.x + dx * sign, position.y + dy * sign,
                                                         position.z + dz * sign);
        if (neighbour == nullptr)
            continue;

        if (neighbour->mName != PALE_OAK_LOG || neighbour->mStates.getString(PILLAR_AXIS_STATE, DEFAULT_AXIS) != axis)
            return false;
    }
    return true;
}

BlockState CreakingHeartBlock::refreshState(Level &level, const Vector3i &position, const BlockState &state,
                                            bool bound) {
    const std::string axis = state.mStates.getString(PILLAR_AXIS_STATE, DEFAULT_AXIS);
    const char *next = !bound && !hasRequiredLogs(level, position, axis) ? UPROOTED
                                                                         : isNaturalNight(level) ? AWAKE : DORMANT;
    if (state.mStates.getString(HEART_STATE) == next)
        return state;

    Tag states = state.mStates;
    states.putString(HEART_STATE, next);
    const BlockState updated(state.mName, states);
    level.setBlock(position, updated, true);
    return updated;
}

bool CreakingHeartBlock::canSpawnCreaking(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                          const BlockState &state) {
    if (state.mStates.getByte(NATURAL_STATE, 0) == 0 || state.mStates.getString(HEART_STATE) != AWAKE)
        return false;

    if (!level.getGameRules().getBool("domobspawning")
        || owner.getProperties().getDifficulty() == Difficulty::Peaceful)
        return false;

    for (auto &entry: owner.getPlayers()) {
        const ServerPlayer &player = entry.second;
        if (!player.isSpawned() || !player.isAlive() || player.getDimension() != level.getDimensionType())
            continue;

        const Vector3f playerPosition = player.getPosition();
        const float dx = playerPosition.x - (float) position.x;
        const float dy = playerPosition.y - (float) position.y;
        const float dz = playerPosition.z - (float) position.z;
        if (dx * dx + dy * dy + dz * dz <= PLAYER_RANGE * PLAYER_RANGE)
            return true;
    }
    return false;
}

bool CreakingHeartBlock::findSpawnPosition(Level &level, const Vector3i &position, Vector3f &spawn) {
    std::uniform_real_distribution<double> offset(-SPAWN_RANGE_HORIZONTAL, SPAWN_RANGE_HORIZONTAL);
    const double x = (double) position.x + offset(heartRandom());
    const double z = (double) position.z + offset(heartRandom());
    const int32_t blockX = (int32_t) std::floor(x);
    const int32_t blockZ = (int32_t) std::floor(z);

    for (int32_t y = position.y - SPAWN_RANGE_VERTICAL; y <= position.y + SPAWN_RANGE_VERTICAL; ++y) {
        const BlockState *ground = level.peekBlockPtr(blockX, y, blockZ);
        if (ground == nullptr || ground->mName == AIR)
            continue;

        if (isAir(level.peekBlockPtr(blockX, y + 1, blockZ)) && isAir(level.peekBlockPtr(blockX, y + 2, blockZ))) {
            spawn = Vector3f((float) x, (float) (y + 1), (float) z);
            return true;
        }
    }
    return false;
}

void CreakingHeartBlock::spawnCreaking(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const Vector3f &spawn) {
    const Vector3f home = centerOf(position);
    owner.spawnActor(level, CREAKING, spawn, [&home](ServerActor &actor) {
        MobActor *mob = dynamic_cast<MobActor *>(&actor);
        if (mob == nullptr)
            return;

        mob->setSpawnEvent(SPAWNED_BY_HEART_EVENT);
        mob->setHomePosition(home);
    });
    owner.playLevelSound(level, SPAWN_SOUND, home);
}

void CreakingHeartBlock::spreadResin(Level &level, const Vector3i &position) {
    const int32_t maximum = RESIN_MIN_COUNT + RandomTickSystem::nextInt(RESIN_EXTRA_COUNT);
    const BlockState resin = VanillaBlocks::RESIN_CLUMP().toBlockState();
    int32_t placed = 0;

    for (int32_t z = position.z - RESIN_SEARCH_RADIUS; z <= position.z + RESIN_SEARCH_RADIUS; ++z) {
        for (int32_t x = position.x - RESIN_SEARCH_RADIUS; x <= position.x + RESIN_SEARCH_RADIUS; ++x) {
            for (int32_t y = position.y - RESIN_SEARCH_RADIUS; y <= position.y + RESIN_SEARCH_RADIUS; ++y) {
                const BlockState *log = level.peekBlockPtr(x, y, z);
                if (log == nullptr || log->mName != PALE_OAK_LOG)
                    continue;

                for (const ResinFace &face: RESIN_FACES) {
                    const Vector3i side(x + face.mX, y + face.mY, z + face.mZ);
                    if (!isAir(level.peekBlockPtr(side.x, side.y, side.z)))
                        continue;

                    Tag states = resin.mStates;
                    states.putInt(MULTI_FACE_STATE, face.mBit);
                    level.setBlock(side, BlockState(resin.mName, states), true);
                    if (++placed >= maximum)
                        return;
                }
            }
        }
    }
}

void CreakingHeartBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                  const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    Block::onPlaced(owner, player, position, state, usedItem, blockFace);

    Level &level = owner.getLevelFor(player);
    refreshState(level, position, state, !findBoundActors(owner, level, position).empty());
    scheduleNextUpdate(level, position);
}

void CreakingHeartBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                            const BlockState &state) const {
    Block::onNeighbourChanged(owner, level, position, state);

    const BlockState current = level.getBlockState(position.x, position.y, position.z);
    if (current.mName != state.mName)
        return;

    refreshState(level, position, current, !findBoundActors(owner, level, position).empty());
    scheduleNextUpdate(level, position);
}

void CreakingHeartBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state) const {
    (void) owner;
    (void) state;

    scheduleNextUpdate(level, position);
}

void CreakingHeartBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                           const BlockState &state) const {
    const bool bound = !findBoundActors(owner, level, position).empty();
    const BlockState current = refreshState(level, position, state, bound);
    scheduleNextUpdate(level, position);
    if (bound || !canSpawnCreaking(owner, level, position, current))
        return;

    for (int32_t attempt = 0; attempt < SPAWN_ATTEMPTS; ++attempt) {
        Vector3f spawn;
        if (findSpawnPosition(level, position, spawn)) {
            spawnCreaking(owner, level, position, spawn);
            return;
        }
    }
}

void CreakingHeartBlock::onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state) const {
    (void) state;

    for (MobActor *mob: findBoundActors(owner, level, position))
        mob->fireEvent(owner, START_TWITCHING_EVENT);
}

bool CreakingHeartBlock::onActorEvent(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state, const std::string &event, MobActor &source) const {
    (void) state;

    if (event == CRUMBLING_EVENT)
        return true;

    if (event != DAMAGED_BY_PLAYER_EVENT)
        return false;

    spreadResin(level, position);
    level.addParticle(CreakingHeartTrailParticle(source.getPosition(), position));
    owner.playLevelSound(level, TRAIL_SOUND, centerOf(position));
    return true;
}
