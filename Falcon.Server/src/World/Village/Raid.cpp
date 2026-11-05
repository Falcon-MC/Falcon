#include "World/Village/Raid.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Village/Village.h"
#include "World/Village/VillageManager.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/OnScreenTextureAnimationPacket.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

namespace {
    const char *const RAID_HORN_SOUND = "raid.horn";
    const char *const START_CELEBRATING_EVENT = "minecraft:start_celebrating";
    const char *const RAID_EXPIRED_EVENT = "minecraft:raid_expired";
    const int32_t HERO_DURATION_TICKS = 48000;
    const float NO_POSITION = std::numeric_limits<float>::max();

    // the level's closest player searches, as squared distances: 128 and 1024 blocks
    const float SPAWN_PLAYER_RANGE = 128.0f;
    const float FAR_PLAYER_RANGE = 1024.0f;

    const int32_t SPAWN_SEARCH_ATTEMPTS = 10;
    const int32_t VILLAGE_SPAWN_RANGE = 8;
    const int32_t GROUP_SCATTER_RANGE = 4;
    const float FARTHEST_PLAYER_MARGIN = 16.0f;
    const float BEYOND_PLAYER_MIN = 32.0f;
    const float BEYOND_PLAYER_SPREAD = 16.0f;
    const float DIRECTION_EPSILON = 0.0001f;

    struct RaidGroupMember {
        const char *mIdentifier;
        int32_t mMinSize;
        int32_t mMaxSize;
        const char *mEvent;
    };

    // definitions/spawn_groups/raid_group_<wave>.json of the 1.26.61 dedicated server, one entry per condition. Each
    // condition has a single permute type, the village_raid mob event, a 0 to 1024 distance filter, spawns on surface
    // and an easy to hard difficulty filter.
    const std::vector<std::vector<RaidGroupMember>> RAID_GROUPS = {
            {{"minecraft:pillager", 4, 4, "minecraft:spawn_for_raid"}},
            {{"minecraft:pillager", 3, 3, "minecraft:spawn_for_raid"},
             {"minecraft:vindicator", 2, 2, "minecraft:spawn_for_raid"}},
            {{"minecraft:pillager", 3, 3, "minecraft:spawn_for_raid"},
             {"minecraft:ravager", 1, 1, "minecraft:spawn_for_raid"}},
            {{"minecraft:witch", 3, 3, "minecraft:spawn_for_raid"},
             {"minecraft:pillager", 3, 3, "minecraft:spawn_for_raid"}},
            {{"minecraft:vindicator", 4, 4, "minecraft:spawn_for_raid"},
             {"minecraft:evocation_illager", 1, 1, "minecraft:spawn_for_raid"},
             {"minecraft:ravager", 1, 1, "minecraft:spawn_for_raid"},
             {"minecraft:ravager", 1, 1, "minecraft:spawn_for_raid_with_pillager_rider"}},
            {{"minecraft:pillager", 5, 5, "minecraft:spawn_for_raid"},
             {"minecraft:vindicator", 2, 2, "minecraft:spawn_for_raid"},
             {"minecraft:evocation_illager", 1, 1, "minecraft:spawn_for_raid"}},
            {{"minecraft:vindicator", 6, 6, "minecraft:spawn_for_raid"},
             {"minecraft:witch", 1, 1, "minecraft:spawn_for_raid"},
             {"minecraft:evocation_illager", 2, 2, "minecraft:spawn_for_raid"},
             {"minecraft:ravager", 1, 1, "minecraft:spawn_for_raid_with_pillager_rider"},
             {"minecraft:ravager", 1, 1, "minecraft:spawn_for_raid_with_evoker_rider"}}
    };

    std::mt19937 &raidRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    float nextFloat() {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(raidRandom());
    }

    // the level random's nextInt: the raw 32 bit draw modulo the bound, 0 for a bound of 0 (0x140391F20)
    uint32_t nextInt(uint32_t bound) {
        return bound == 0 ? 0 : (uint32_t) raidRandom()() % bound;
    }

    float distanceSquared(const Vector3f &a, const Vector3f &b) {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

    ServerPlayer *closestPlayer(ServerNetworkHandler &owner, Level &level, const Vector3f &position, float range) {
        ServerPlayer *closest = nullptr;
        float best = range * range;
        for (auto &entry: owner.getPlayers()) {
            ServerPlayer &player = entry.second;
            if (!player.isSpawned() || !player.isIn(level))
                continue;

            const float distance = distanceSquared(player.getPosition(), position);
            if (distance < best) {
                best = distance;
                closest = &player;
            }
        }
        return closest;
    }

    void fireOn(ServerNetworkHandler &owner, int64_t id, const char *event) {
        if (MobActor *mob = VillageManager::findMob(owner, id))
            mob->fireEvent(owner, event);
    }

    // vanilla reads air outside loaded chunks; a block that is not loaded here is never a place to stand
    bool canStandOn(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockState *ground = level.peekBlockPtr(x, y, z);
        const BlockState *body = level.peekBlockPtr(x, y + 1, z);
        const BlockState *head = level.peekBlockPtr(x, y + 2, z);
        if (ground == nullptr || body == nullptr || head == nullptr)
            return false;

        // material 7, leaves, is rejected even though it collides
        const Block *block = VanillaBlocks::fromState(*ground);
        return (block == nullptr || !block->isLeaves()) && BlockShape::hasCollision(*ground)
               && !BlockShape::hasCollision(*body) && !BlockShape::hasCollision(*head);
    }

    /**
     * sub_1439CE4F0: a random column around the origin, then that column scanned down and up for a block to stand
     * on. The result is the air block above the ground.
     */
    bool findStandingPosition(Level &level, const Vector3f &origin, int32_t attempts, int32_t horizontalRange,
                              int32_t verticalRange, Vector3f &result) {
        const int32_t originX = (int32_t) std::floor(origin.x);
        const int32_t originY = (int32_t) std::floor(origin.y);
        const int32_t originZ = (int32_t) std::floor(origin.z);
        for (int32_t attempt = 0; attempt < attempts; ++attempt) {
            // x is drawn first, then z, then y
            const int32_t rollX = horizontalRange >= 0 ? (int32_t) nextInt((uint32_t) (2 * horizontalRange + 1)) : 0;
            const int32_t rollZ = horizontalRange >= 0 ? (int32_t) nextInt((uint32_t) (2 * horizontalRange + 1)) : 0;
            const int32_t rollY = verticalRange >= 0 ? (int32_t) nextInt((uint32_t) (2 * verticalRange + 1)) : 0;
            const int32_t x = originX + rollX - horizontalRange;
            const int32_t z = originZ + rollZ - horizontalRange;
            const int32_t start = originY + rollY - verticalRange;

            bool found = canStandOn(level, x, start, z);
            int32_t ground = start;
            if (!found && start >= originY - verticalRange) {
                for (int32_t y = start - 1;; --y) {
                    if (canStandOn(level, x, y, z)) {
                        found = true;
                        ground = y;
                        break;
                    }
                    if (y <= originY - verticalRange - 1)
                        break;
                }
            }
            if (!found && start + 1 < originY + verticalRange) {
                for (int32_t y = start + 1;; ++y) {
                    if (canStandOn(level, x, y, z)) {
                        found = true;
                        ground = y;
                        break;
                    }
                    if (y + 1 >= originY + verticalRange)
                        break;
                }
            }
            if (!found)
                continue;

            result = Vector3f((float) x, (float) (ground + 1), (float) z);
            return true;
        }
        return false;
    }

    /**
     * sub_1439D8D80: a random point on the edge of the village box, pushed along the direction from the centre. With
     * fromEdge it starts on the edge and moves distance further out, otherwise it sits distance * scale from the
     * floored centre.
     */
    bool findSpawnAroundVillage(ServerNetworkHandler &owner, Level &level, const Village &village, float distance,
                                float scale, bool onHeightmap, bool fromEdge, Vector3f &result) {
        const AxisAlignedBB &bounds = village.getBounds();
        const Vector3f center = village.getCenter();
        const Vector3f origin(std::floor(center.x), std::floor(center.y), std::floor(center.z));
        for (int32_t attempt = 0; attempt < SPAWN_SEARCH_ATTEMPTS; ++attempt) {
            const float width = bounds.mMaxX - bounds.mMinX;
            const float depth = bounds.mMaxZ - bounds.mMinZ;
            float along = nextFloat() * ((depth + depth) + (width + width));

            // the perimeter is walked from the max z edge, then max x, min z and min x
            float dx = 0.0f;
            float dz = 0.0f;
            if (along <= width) {
                dx = (along + bounds.mMinX) - center.x;
                dz = bounds.mMaxZ - center.z;
            } else {
                int32_t edge = -1;
                float current = width;
                float next;
                do {
                    next = (edge & 1) == 0 ? width : depth;
                    along -= current;
                    ++edge;
                    current = next;
                } while (along > next);

                if (edge == 0) {
                    dx = bounds.mMaxX - center.x;
                    dz = (bounds.mMaxZ - along) - center.z;
                } else if (edge == 1) {
                    dx = (bounds.mMaxX - along) - center.x;
                    dz = bounds.mMinZ - center.z;
                } else if (edge == 2) {
                    dx = bounds.mMinX - center.x;
                    dz = (along + bounds.mMinZ) - center.z;
                }
            }

            const float length = std::sqrt(dx * dx + dz * dz);
            const float directionX = length >= DIRECTION_EPSILON ? dx / length : 0.0f;
            const float directionZ = length >= DIRECTION_EPSILON ? dz / length : 0.0f;
            const float x = fromEdge ? dx * scale + directionX * distance : directionX * distance * scale;
            const float z = fromEdge ? dz * scale + directionZ * distance : directionZ * distance * scale;
            Vector3f candidate(x + origin.x, origin.y, z + origin.z);

            // the game takes its block source from the closest player within 64 blocks, else within 1024
            // (sub_1439D88C0): only whether one exists matters here
            if (closestPlayer(owner, level, candidate, FAR_PLAYER_RANGE) == nullptr)
                continue;

            if (onHeightmap) {
                const int32_t blockX = (int32_t) std::floor(candidate.x);
                const int32_t blockZ = (int32_t) std::floor(candidate.z);
                candidate = Vector3f((float) blockX, (float) level.getHeightAt(blockX, blockZ), (float) blockZ);
            }

            Vector3f standing;
            if (!findStandingPosition(level, candidate, SPAWN_SEARCH_ATTEMPTS, VILLAGE_SPAWN_RANGE,
                                      VILLAGE_SPAWN_RANGE, standing))
                continue;

            if (closestPlayer(owner, level, standing, SPAWN_PLAYER_RANGE) == nullptr)
                continue;

            result = standing;
            return true;
        }
        return false;
    }

    /**
     * The second strategy of the spawn search (0x1439f44ae): the player inside the village box grown by 16 that is
     * farthest from the centre, then a point 32 to 48 blocks from that player on the side away from the village.
     */
    bool findSpawnBeyondFarthestPlayer(ServerNetworkHandler &owner, Level &level, const Village &village,
                                       Vector3f &result) {
        const AxisAlignedBB area = village.getBounds().expand(FARTHEST_PLAYER_MARGIN, FARTHEST_PLAYER_MARGIN,
                                                              FARTHEST_PLAYER_MARGIN);
        const Vector3f center = village.getCenter();
        ServerPlayer *farthest = nullptr;
        float best = std::numeric_limits<float>::min();
        for (auto &entry: owner.getPlayers()) {
            ServerPlayer &player = entry.second;
            if (!player.isSpawned() || !player.isIn(level))
                continue;

            const Vector3f &position = player.getPosition();
            if (std::fmin(area.mMinX, area.mMaxX) > position.x || position.x > std::fmax(area.mMinX, area.mMaxX)
                || std::fmin(area.mMinY, area.mMaxY) > position.y || position.y > std::fmax(area.mMinY, area.mMaxY)
                || std::fmin(area.mMinZ, area.mMaxZ) > position.z || position.z > std::fmax(area.mMinZ, area.mMaxZ))
                continue;

            const float distance = distanceSquared(position, center);
            if (distance > best) {
                best = distance;
                farthest = &player;
            }
        }
        if (farthest == nullptr)
            return false;

        const Vector3f origin = farthest->getPosition();
        const float awayX = origin.x - center.x;
        const float awayZ = origin.z - center.z;
        const float length = std::sqrt(awayX * awayX + awayZ * awayZ);
        const float directionX = length >= DIRECTION_EPSILON ? awayX / length : 0.0f;
        const float directionZ = length >= DIRECTION_EPSILON ? awayZ / length : 0.0f;
        for (int32_t attempt = 0; attempt < SPAWN_SEARCH_ATTEMPTS; ++attempt) {
            const float radius = nextFloat() * BEYOND_PLAYER_SPREAD + BEYOND_PLAYER_MIN;
            // the roll is used as radians, without the degree conversion
            const float angle = nextFloat() * 360.0f;
            float x = origin.x + std::cos(angle) * radius;
            float z = origin.z + std::sin(angle) * radius;

            // a point on the village side of the player is mirrored to the other side
            const float along = directionX * (x - origin.x) + directionZ * (z - origin.z);
            if (along < 0.0f) {
                x -= directionX * (along + along);
                z -= directionZ * (along + along);
            }

            Vector3f standing;
            if (findStandingPosition(level, Vector3f(x, center.y, z), SPAWN_SEARCH_ATTEMPTS, VILLAGE_SPAWN_RANGE,
                                     VILLAGE_SPAWN_RANGE, standing)) {
                result = standing;
                return true;
            }
        }
        return false;
    }

    // sub_1439DF120: the horn is played through the closest player within 1024 blocks, and not at all without one
    void soundHorn(ServerNetworkHandler &owner, Level &level, const Vector3f &position) {
        if (closestPlayer(owner, level, position, FAR_PLAYER_RANGE) != nullptr)
            owner.playLevelSound(level, RAID_HORN_SOUND, position);
    }

    /**
     * Spawner::spawnMobGroup (BedrockSpawner vtable 0x14F1887F0 + 0x80, sub_1432C1140) for a raid group: every member
     * spawns at the floored position one block up, or scattered around it when the area is loaded, and the herd event
     * is fired once the whole condition has spawned.
     */
    std::vector<int64_t> spawnRaidGroup(ServerNetworkHandler &owner, Level &level, uint8_t wave,
                                        const Vector3f &position, bool scatter) {
        std::vector<int64_t> spawned;
        if (wave >= RAID_GROUPS.size())
            return spawned;

        for (const RaidGroupMember &member: RAID_GROUPS[wave]) {
            // squaring the roll biases the herd towards its minimum size (0x1432C1E8F)
            const float roll = nextFloat();
            const int32_t count = member.mMinSize
                                  + (int32_t) std::round((float) (member.mMaxSize - member.mMinSize) * (roll * roll));

            std::vector<MobActor *> herd;
            for (int32_t index = 0; index < count; ++index) {
                Vector3f at(std::floor(position.x), std::floor(position.y) + 1.0f, std::floor(position.z));
                Vector3f standing;
                if (scatter && findStandingPosition(level, at, SPAWN_SEARCH_ATTEMPTS, GROUP_SCATTER_RANGE,
                                                    GROUP_SCATTER_RANGE, standing))
                    at = standing;

                MobActor *mob = dynamic_cast<MobActor *>(owner.spawnActor(level, member.mIdentifier, at));
                if (mob == nullptr)
                    continue;

                mob->startDefinition(owner);
                herd.push_back(mob);
            }

            for (MobActor *mob: herd) {
                mob->fireEvent(owner, member.mEvent);
                spawned.push_back(mob->getUniqueId());
            }
        }
        return spawned;
    }

    // sub_1439D88C0 lets the group scatter only when the chunks within the radius are loaded
    bool isAreaLoaded(Level &level, const Vector3f &position, int32_t radius) {
        const int32_t minX = (int32_t) std::floor(position.x - (float) radius) >> 4;
        const int32_t maxX = (int32_t) std::floor(position.x + (float) radius) >> 4;
        const int32_t minZ = (int32_t) std::floor(position.z - (float) radius) >> 4;
        const int32_t maxZ = (int32_t) std::floor(position.z + (float) radius) >> 4;
        for (int32_t chunkX = minX; chunkX <= maxX; ++chunkX) {
            for (int32_t chunkZ = minZ; chunkZ <= maxZ; ++chunkZ) {
                if (!level.isChunkPopulated(chunkX, chunkZ))
                    return false;
            }
        }
        return true;
    }

    // Village::_createRaid lambda (0x1439F3760): the village is lost without beds or inhabitants
    bool isVillageLost(const Village &village) {
        return village.countBeds() == 0 || village.getInhabitantCount() == 0;
    }
}

Raid::Raid(uint8_t waveCount) : mWaveCount(waveCount) {
}

void Raid::tick(ServerNetworkHandler &owner, Level &level, Village &village, int64_t currentTick) {
    if (mLastTick != -1 && mLastTick >= currentTick)
        return;

    mLastTick = currentTick;
    const int64_t elapsed = mTicksInState;
    ++mTicksInState;

    switch (mState) {
        case State::Cooldown:
            if (mTicksInState >= mCooldownTicks) {
                mState = State::FindingSpawn;
                _resetStateTimers();
            }
            break;
        case State::FindingSpawn: {
            Vector3f position;
            if (!_findSpawn(owner, level, village, elapsed, position))
                break;

            if (position.x != NO_POSITION || position.y != NO_POSITION || position.z != NO_POSITION) {
                mSpawnPosition = position;
                mState = State::SpawningWave;
            } else {
                mState = State::Removed;
            }
            _resetStateTimers();
            break;
        }
        case State::SpawningWave:
            if (!_spawnWave(owner, level, village, elapsed))
                break;

            if (!mRaiders.empty()) {
                mTotalHealth = _sumRaiderHealth(owner);
                mSpawnFailures = 0;
                _onWaveSpawned(owner, level);
                mState = State::Ongoing;
            } else {
                ++mSpawnFailures;
                mState = mSpawnFailures <= mMaxSpawnFailures ? State::FindingSpawn : State::Removed;
            }
            _resetStateTimers();
            break;
        case State::Ongoing: {
            // a raider stays valid while the village counts it as a hostile dweller (0x1439F3870)
            for (auto it = mRaiders.begin(); it != mRaiders.end();) {
                if (village.hasDweller(DwellerRole::Hostile, *it)) {
                    ++it;
                    continue;
                }

                fireOn(owner, *it, RAID_EXPIRED_EVENT);
                it = mRaiders.erase(it);
            }

            if (!mRaiders.empty()) {
                if (mHornCountdown-- <= 1) {
                    mHornCountdown = mHornInterval;
                    _soundHornNearRaider(owner, level);
                }
            } else if (mNextWaveCountdown-- <= 0) {
                ++mCurrentWave;
                mState = mCurrentWave >= mWaveCount ? State::Victory : State::Cooldown;
                _resetStateTimers();
            }

            // checked even on the tick a wave is cleared, so a lost village overrides the victory
            if (isVillageLost(village)) {
                mState = State::Ended;
                mResult = Result::Defeat;
                _resetStateTimers();
                _onDefeat(owner);
            }
            break;
        }
        case State::Victory:
            _onVictory(owner, level, village);
            for (int64_t hero: mHeroes)
                _rewardHero(owner, hero);
            mHeroes.clear();
            mState = State::Ended;
            mResult = Result::Victory;
            _resetStateTimers();
            break;
        case State::Ended:
            if (mTicksInState >= mCelebrationTicks)
                mState = State::Removed;
            break;
        case State::Removed:
            break;
    }
}

void Raid::expireRaiders(ServerNetworkHandler &owner) const {
    for (int64_t id: mRaiders)
        fireOn(owner, id, RAID_EXPIRED_EVENT);
}

float Raid::getProgress(ServerNetworkHandler &owner) const {
    switch (mState) {
        case State::Cooldown:
            if (mCooldownTicks <= 0)
                return 1.0f;
            return std::min(1.0f, std::max(0.0f, (float) mTicksInState / (float) mCooldownTicks));
        case State::FindingSpawn:
        case State::SpawningWave:
            return 1.0f;
        case State::Ongoing:
        case State::Victory:
        case State::Ended:
        case State::Removed:
            if (mTotalHealth <= 0.0f)
                return 0.0f;
            return std::min(1.0f, std::max(0.0f, _sumRaiderHealth(owner) / mTotalHealth));
    }
    return 0.0f;
}

void Raid::_resetStateTimers() {
    mTicksInState = 0;
    mHornCountdown = mHornInterval;
    mNextWaveCountdown = mNextWaveDelay;
}

float Raid::_sumRaiderHealth(ServerNetworkHandler &owner) const {
    // each raider counts its health truncated to an int (0x1439F51B0)
    float total = 0.0f;
    for (int64_t id: mRaiders) {
        if (const MobActor *mob = VillageManager::findMob(owner, id))
            total += (float) (int32_t) mob->getHealth();
    }
    return total;
}

bool Raid::_findSpawn(ServerNetworkHandler &owner, Level &level, const Village &village, int64_t elapsed,
                      Vector3f &position) const {
    // one strategy per tick; after the sixth the raid gives up (0x1439F43C0)
    switch (elapsed) {
        case 0:
            return findSpawnAroundVillage(owner, level, village, 16.0f, 1.0f, false, true, position);
        case 1:
            return findSpawnBeyondFarthestPlayer(owner, level, village, position);
        case 2:
            return findSpawnAroundVillage(owner, level, village, 0.0f, 1.0f, false, true, position);
        case 3:
            return findSpawnAroundVillage(owner, level, village, 1.0f, 0.5f, false, false, position);
        case 4:
            return findSpawnAroundVillage(owner, level, village, 8.0f, 1.0f, false, false, position);
        case 5:
            return findSpawnAroundVillage(owner, level, village, 8.0f, 1.0f, true, false, position);
        default:
            position = Vector3f(NO_POSITION, NO_POSITION, NO_POSITION);
            return true;
    }
}

bool Raid::_spawnWave(ServerNetworkHandler &owner, Level &level, Village &village, int64_t elapsed) {
    // Village::_spawnRaidGroup (0x1439F3A10): the group spawns on the first tick, and the second binds every raider
    // and the first rider of each to the village
    if (elapsed == 1) {
        std::vector<int64_t> riders;
        for (int64_t id: mRaiders) {
            MobActor *raider = VillageManager::findMob(owner, id);
            if (raider == nullptr)
                continue;

            raider->getDweller().joinVillage(owner, *raider, village);
            if (raider->getPassengers().empty())
                continue;

            MobActor *rider = VillageManager::findMob(owner, raider->getPassengers().front());
            if (rider == nullptr)
                continue;

            riders.push_back(rider->getUniqueId());
            rider->getDweller().joinVillage(owner, *rider, village);
        }
        mRaiders.insert(riders.begin(), riders.end());
        return true;
    }

    if (elapsed != 0)
        return true;

    if (closestPlayer(owner, level, mSpawnPosition, FAR_PLAYER_RANGE) == nullptr)
        return false;

    const bool scatter = isAreaLoaded(level, mSpawnPosition, GROUP_SCATTER_RANGE);
    const std::vector<int64_t> spawned = spawnRaidGroup(owner, level, mCurrentWave, mSpawnPosition, scatter);
    mRaiders.insert(spawned.begin(), spawned.end());
    return false;
}

void Raid::_onWaveSpawned(ServerNetworkHandler &owner, Level &level) const {
    soundHorn(owner, level, mSpawnPosition);
}

void Raid::_onVictory(ServerNetworkHandler &owner, Level &level, const Village &village) const {
    // the hero animation is shown to everyone in the raid area, rewarded or not (0x1439F4BB0)
    OnScreenTextureAnimationPacket animation;
    animation.mEffectId = (uint32_t) MobEffectId::VillageHero;
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (player.isSpawned() && player.isIn(level) && village.isInRaidBounds(player.getPosition()))
            owner.sendPacketTo(player.getNetworkIdentifier(), animation);
    }

    for (int64_t id: village.getDwellerIds(DwellerRole::Inhabitant))
        fireOn(owner, id, START_CELEBRATING_EVENT);
}

void Raid::_onDefeat(ServerNetworkHandler &owner) const {
    for (int64_t id: mRaiders)
        fireOn(owner, id, START_CELEBRATING_EVENT);
}

void Raid::_rewardHero(ServerNetworkHandler &owner, int64_t playerId) const {
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (player.getUniqueId() != playerId || !player.isSpawned() || !player.isAlive())
            continue;

        owner.addEffect(player, MobEffectId::VillageHero, 0, HERO_DURATION_TICKS);
        return;
    }
}

void Raid::_soundHornNearRaider(ServerNetworkHandler &owner, Level &level) const {
    std::vector<Vector3f> positions;
    for (int64_t id: mRaiders) {
        if (const MobActor *raider = VillageManager::findMob(owner, id))
            positions.push_back(raider->getPosition());
    }
    if (positions.empty())
        return;

    soundHorn(owner, level, positions[raidRandom()() % positions.size()]);
}
