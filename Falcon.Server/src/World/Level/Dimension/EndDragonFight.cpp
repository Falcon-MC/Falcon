#include "World/Level/Dimension/EndDragonFight.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Misc/EndCrystalActor.h"
#include "World/Actor/Mob/Hostile/EnderDragon.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Level/Block/Actor/EndGatewayBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Dimension/EndGatewayExit.h"
#include "World/Level/Explosion.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/End/Feature/EndSpikes.h"
#include "World/Level/LevelGen/End/Feature/TheEndDecorationFeature.h"
#include "World/Level/LevelGen/End/Object/EndPodiumObject.h"
#include "World/Level/LevelGen/End/TheEndGenerator.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

#include <cmath>
#include <limits>
#include <random>
#include <utility>

namespace {

    const char *const DRAGON_FIGHT_TAG = "DragonFight";
    const char *const DRAGON_UNIQUE_ID_TAG = "DragonUUID";
    const char *const DRAGON_KILLED_TAG = "DragonKilled";
    const char *const DRAGON_SPAWNED_TAG = "DragonSpawned";
    const char *const PREVIOUSLY_KILLED_TAG = "PreviouslyKilled";
    const char *const IS_RESPAWNING_TAG = "IsRespawning";
    const char *const VERSION_TAG = "DragonFightVersion";
    const char *const EXIT_PORTAL_TAG = "ExitPortalLocation";
    const char *const GATEWAYS_TAG = "Gateways";

    const uint8_t CURRENT_VERSION = 1;

    const int32_t CHECK_INTERVAL = 100;
    const int32_t PILLAR_PREPARE_TICKS = 100;
    const int32_t PILLAR_PREPARE_SOUND_FROM = 95;
    const int32_t TICKS_PER_PILLAR = 40;
    const int32_t PILLAR_CLEAR_TICK = 38;
    const int32_t PILLAR_REBUILD_TICK = 39;
    const int32_t DRAGON_SUMMON_TICKS = 100;
    const int32_t DRAGON_SUMMON_SOUND_FROM = 80;
    const int32_t DRAGON_SUMMON_EARLY_SOUND_UNTIL = 4;
    const int32_t SPIKE_CLEAR_RADIUS = 10;
    const float SPIKE_EXPLOSION_SIZE = 5.0f;
    const float RESPAWN_CRYSTAL_EXPLOSION_SIZE = 6.0f;
    const float DRAGON_SEARCH_RADIUS = 200.0f;
    const int32_t RESPAWN_CRYSTAL_OFFSET = 3;
    const int32_t ARENA_CHUNK_RADIUS = 1;
    const int32_t PODIUM_PILLAR_HEIGHT = 3;
    const int32_t PODIUM_BEDROCK_COLUMN = 5;

    const char *const SUMMON_SOUND = "mad";

    // the blocks around each end of a gateway its searches read, island and return search included
    const int32_t GATEWAY_END_REACH = 80;

    // keeps both ends of a gateway loaded while its exit is found; players use their runtime id, ticking areas bit 63
    const uint64_t GATEWAY_LOADER_ID = (1ULL << 62) | 0x47415445ULL;

    bool isAir(const BlockState *state) {
        return state == nullptr || state->mName == "minecraft:air";
    }

    bool isBedrock(const BlockState *state) {
        return state != nullptr && state->mName == "minecraft:bedrock";
    }

    void setIfChanged(Level &level, int32_t x, int32_t y, int32_t z, const BlockState &state) {
        if (y < level.getMinY() || y > level.getMaxY())
            return;

        const BlockState *current = level.peekBlockPtr(x, y, z);
        if (current != nullptr && *current == state)
            return;

        level.setBlock(Vector3i(x, y, z), state, false);
    }

    // places generated objects straight into the live level, sending each changed block
    class LiveBlockManager final : public BlockManager {
    public:
        explicit LiveBlockManager(Level &level) : BlockManager(level) {
        }

        void setBlockStateAt(int32_t x, int32_t y, int32_t z, const BlockState &state) override {
            setIfChanged(getLevel(), x, y, z, state);
        }
    };

    // the highest block of the column above y, or below the world when the column is empty
    int32_t topOfColumn(Level &level, int32_t x, int32_t z) {
        for (int32_t y = level.getMaxY(); y >= level.getMinY(); --y) {
            if (!isAir(level.peekBlockPtr(x, y, z)))
                return y;
        }

        return level.getMinY() - 1;
    }

    EndGatewayExit::BlockReader levelReader(Level &level) {
        return [&level](int32_t x, int32_t y, int32_t z) {
            return level.peekBlockPtr(x, y, z);
        };
    }

    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y, (float) position.z + 0.5f);
    }

    Tag intList(const std::vector<int32_t> &values) {
        Tag list = Tag::ofList(Tag::Type::Int);
        for (const int32_t value: values)
            list.addToList(Tag::ofInt(value));
        return list;
    }

    int32_t listInt(const Tag &list, size_t index) {
        const std::vector<Tag> &values = list.getList();
        if (index >= values.size() || values[index].getType() != Tag::Type::Int)
            return 0;

        return values[index].asInt();
    }

}

EndDragonFight::EndDragonFight(int64_t seed) : mSeed(seed) {
}

EndDragonFight::RespawnStep EndDragonFight::stepRespawn(RespawnStage stage, int32_t ticks, size_t spikeCount) {
    RespawnStep step;
    step.mNextStage = stage;

    switch (stage) {
        case RespawnStage::Start:
            step.mBeamToSummonPoint = true;
            step.mNextStage = RespawnStage::PreparingToSummonPillars;
            step.mRestartTimer = true;
            break;
        case RespawnStage::PreparingToSummonPillars:
            if (ticks >= PILLAR_PREPARE_TICKS) {
                step.mNextStage = RespawnStage::SummoningPillars;
                step.mRestartTimer = true;
                break;
            }

            step.mPlaySummonSound = ticks >= PILLAR_PREPARE_SOUND_FROM || ticks == 0 || ticks == 50 || ticks == 51
                                    || ticks == 52;
            break;
        case RespawnStage::SummoningPillars: {
            const int32_t spike = ticks / TICKS_PER_PILLAR;
            const int32_t phase = ticks % TICKS_PER_PILLAR;
            if ((size_t) spike >= spikeCount) {
                if (phase == 0) {
                    step.mNextStage = RespawnStage::SummoningDragon;
                    step.mRestartTimer = true;
                }
                break;
            }

            if (phase == 0)
                step.mBeamToSpike = spike;
            else if (phase == PILLAR_CLEAR_TICK)
                step.mClearSpike = spike;
            else if (phase == PILLAR_REBUILD_TICK)
                step.mRebuildSpike = spike;
            break;
        }
        case RespawnStage::SummoningDragon:
            if (ticks >= DRAGON_SUMMON_TICKS) {
                step.mSummonDragon = true;
                step.mNextStage = RespawnStage::None;
                step.mRestartTimer = true;
            } else if (ticks >= DRAGON_SUMMON_SOUND_FROM) {
                step.mPlaySummonSound = true;
            } else if (ticks == 0) {
                step.mBeamToSummonPoint = true;
            } else if (ticks <= DRAGON_SUMMON_EARLY_SOUND_UNTIL) {
                step.mPlaySummonSound = true;
            }
            break;
        default:
            break;
    }

    return step;
}

std::vector<int32_t> EndDragonFight::createGatewayOrder(int64_t seed) {
    std::vector<int32_t> gateways;
    gateways.reserve(GATEWAY_COUNT);
    for (int32_t i = 0; i < GATEWAY_COUNT; i++)
        gateways.push_back(i);

    std::mt19937 random((uint32_t) seed);
    for (size_t i = 1; i < gateways.size(); i++) {
        const size_t other = random() % (uint32_t) (i + 1);
        if (other != i)
            std::swap(gateways[i], gateways[other]);
    }

    return gateways;
}

Vector3i EndDragonFight::getGatewayPosition(int32_t index) {
    const float step = (float) index * 0.15707964f + -3.1415927f;
    const float angle = step + step;
    return Vector3i((int32_t) (std::cos(angle) * (float) GATEWAY_RADIUS), GATEWAY_Y,
                    (int32_t) (std::sin(angle) * (float) GATEWAY_RADIUS));
}

int32_t EndDragonFight::getDeathExperience(int32_t deathTicks, bool previouslyKilled) {
    const int32_t reward = previouslyKilled ? REPEAT_KILL_EXPERIENCE : FIRST_KILL_EXPERIENCE;
    int32_t experience = 0;
    if (deathTicks >= 151 && deathTicks % 5 == 0)
        experience += (int32_t) std::floor((float) reward * 0.08f);
    if (deathTicks >= DEATH_DURATION)
        experience += (int32_t) std::floor((float) reward * 0.2f);
    return experience;
}

Vector3i EndDragonFight::getSummonPoint() {
    return Vector3i(0, 128, 0);
}

void EndDragonFight::load(const Tag &dimensionData) {
    const Tag *fight = dimensionData.get(DRAGON_FIGHT_TAG);
    if (fight == nullptr || !fight->isCompound())
        return;

    if (!fight->contains(DRAGON_KILLED_TAG)) {
        mDragonKilled = false;
        mPreviouslyKilled = false;
        mDragonSpawned = false;
    } else {
        mDragonUniqueId = fight->getLong(DRAGON_UNIQUE_ID_TAG, 0);
        mDragonKilled = fight->getByte(DRAGON_KILLED_TAG) != 0;
        mDragonSpawned = fight->getByte(DRAGON_SPAWNED_TAG) != 0;
        mPreviouslyKilled = fight->getByte(PREVIOUSLY_KILLED_TAG) != 0;

        if (fight->contains(VERSION_TAG))
            mVersion = (uint8_t) fight->getByte(VERSION_TAG);

        // a fight saved before the version tag forgets the earlier kill, like sub_147170120 does
        if (mVersion == 0) {
            mPreviouslyKilled = false;
            mVersion = CURRENT_VERSION;
        }

        if (fight->getByte(IS_RESPAWNING_TAG) != 0) {
            mRespawnStage = RespawnStage::Start;
            mRespawnTicks = 0;
        }

        const Tag *exit = fight->get(EXIT_PORTAL_TAG);
        if (exit != nullptr && exit->isList())
            mExitPortalLocation = Vector3i(listInt(*exit, 0), listInt(*exit, 1), listInt(*exit, 2));
    }

    mGateways.clear();
    const Tag *gateways = fight->get(GATEWAYS_TAG);
    if (gateways == nullptr || !gateways->isList()) {
        mGateways = createGatewayOrder(mSeed);
        return;
    }

    for (const Tag &gateway: gateways->getList())
        mGateways.push_back(gateway.getType() == Tag::Type::Int ? gateway.asInt() : 0);
}

void EndDragonFight::save(Tag &dimensionData) const {
    Tag fight = Tag::ofCompound();
    fight.putLong(DRAGON_UNIQUE_ID_TAG, mDragonUniqueId);
    fight.putBool(DRAGON_KILLED_TAG, mDragonKilled);
    fight.putBool(DRAGON_SPAWNED_TAG, mDragonSpawned);
    fight.putBool(PREVIOUSLY_KILLED_TAG, mPreviouslyKilled);
    fight.putBool(IS_RESPAWNING_TAG, mRespawnStage != RespawnStage::None);
    fight.putByte(VERSION_TAG, (int8_t) mVersion);

    if (mExitPortalLocation.has_value()) {
        fight.put(EXIT_PORTAL_TAG,
                  intList({mExitPortalLocation->x, mExitPortalLocation->y, mExitPortalLocation->z}));
    }

    fight.put(GATEWAYS_TAG, intList(mGateways.empty() ? createGatewayOrder(mSeed) : mGateways));
    dimensionData.put(DRAGON_FIGHT_TAG, fight);
}

std::vector<int32_t> &EndDragonFight::_gatewayOrder() {
    if (mGateways.empty())
        mGateways = createGatewayOrder(mSeed);
    return mGateways;
}

bool EndDragonFight::_isArenaLoaded(Level &level) const {
    for (int32_t chunkX = -ARENA_CHUNK_RADIUS; chunkX <= ARENA_CHUNK_RADIUS; chunkX++) {
        for (int32_t chunkZ = -ARENA_CHUNK_RADIUS; chunkZ <= ARENA_CHUNK_RADIUS; chunkZ++) {
            if (!level.isChunkPopulated(chunkX, chunkZ))
                return false;
        }
    }

    const Vector3i summon = getSummonPoint();
    return level.isChunkPopulated(summon.x >> 4, summon.z >> 4);
}

void EndDragonFight::tick(ServerNetworkHandler &owner, Level &level) {
    _tickGatewayRequests(owner, level);

    if (!_isArenaLoaded(level))
        return;

    if (mRespawnStage != RespawnStage::None) {
        _tickRespawn(owner, level);
        return;
    }

    if (!mDragonKilled) {
        _tickDragon(owner, level);
        return;
    }

    // a killed fight keeps its exit portal, rebuilding it when it is gone
    if (++mPortalCheckTicks < CHECK_INTERVAL)
        return;

    mPortalCheckTicks = 0;
    if (!mExitPortalLocation.has_value()) {
        _spawnExitPortal(owner, level, true);
        return;
    }

    const Vector3i ring(mExitPortalLocation->x + 1, mExitPortalLocation->y, mExitPortalLocation->z);
    const BlockState *portal = level.peekBlockPtr(ring.x, ring.y, ring.z);
    if (portal != nullptr && portal->mName != VanillaBlocks::END_PORTAL().toBlockState().mName)
        _spawnExitPortal(owner, level, true);
}

void EndDragonFight::_tickDragon(ServerNetworkHandler &owner, Level &level) {
    EnderDragon *dragon = _findDragon(owner, level);
    if (dragon != nullptr) {
        _attachDragon(*dragon);
    } else if (!mDragonSpawned || !level.hasStoredEntity(mDragonUniqueId)) {
        // a spawned dragon saved with a chunk that is not loaded is waited for instead (sub_1432BAB90)
        _initializeDragon(owner, level);
        _spawnExitPortal(owner, level, false);
    }

    if (++mDragonCheckTicks >= CHECK_INTERVAL) {
        _updateCrystalCount(owner, level);
        mDragonCheckTicks = 0;
    }
}

EnderDragon *EndDragonFight::_findDragon(ServerNetworkHandler &owner, Level &level) {
    if (mDragonUniqueId != -1) {
        // the tracked dragon still counts while it dies, or a second one would spawn (sub_14716C8B0)
        EnderDragon *known = dynamic_cast<EnderDragon *>(owner.getActor(mDragonUniqueId));
        if (known != nullptr && known->isIn(level))
            return known;
    }

    for (auto &entry: owner.getActors()) {
        EnderDragon *dragon = dynamic_cast<EnderDragon *>(entry.second.get());
        if (dragon == nullptr || !dragon->isIn(level) || dragon->isDead())
            continue;

        const Vector3f position = dragon->getPosition();
        if (std::fabs(position.x) <= DRAGON_SEARCH_RADIUS && std::fabs(position.z) <= DRAGON_SEARCH_RADIUS)
            return dragon;
    }

    return nullptr;
}

void EndDragonFight::_attachDragon(EnderDragon &dragon) {
    // the dragon learns the crystal count only when one is destroyed (sub_1471697A0, sub_14716B290)
    dragon.setPreviouslyKilled(mPreviouslyKilled);
    mDragonUniqueId = dragon.getUniqueId();
    mDragonSpawned = true;
}

void EndDragonFight::_initializeDragon(ServerNetworkHandler &owner, Level &level) {
    std::mt19937 random((uint32_t) (mSeed ^ (int64_t) owner.getCurrentTick()));
    const float yaw = std::uniform_real_distribution<float>(0.0f, 1.0f)(random) * 360.0f;
    const Vector3i summon = getSummonPoint();

    ServerActor *spawned = owner.spawnActor(level, EnderDragon::IDENTIFIER,
                                            Vector3f((float) summon.x, (float) summon.y, (float) summon.z),
                                            [yaw](ServerActor &actor) {
                                                actor.setRotation(Vector3f(0.0f, yaw, yaw));
                                            });
    EnderDragon *dragon = dynamic_cast<EnderDragon *>(spawned);
    if (dragon != nullptr)
        _attachDragon(*dragon);

    // _initializeDragon puts back the obsidian of a spike that is gone, without its crystal (sub_1471E91E0)
    for (const EndSpike &spike: EndSpikes::create((uint32_t) mSeed)) {
        bool standing = true;
        for (int32_t x = -spike.mRadius; x <= spike.mRadius && standing; x++) {
            for (int32_t z = -spike.mRadius; z <= spike.mRadius && standing; z++) {
                if (x * x + z * z > spike.mRadius * spike.mRadius + 1)
                    continue;

                const BlockState *state = level.peekBlockPtr(spike.mX + x, spike.mHeight - 1, spike.mZ + z);
                standing = state == nullptr || state->mName == "minecraft:obsidian";
            }
        }

        if (standing)
            continue;

        EndSpikes::place(spike, false, [&level](int32_t x, int32_t y, int32_t z, const BlockState &state) {
            setIfChanged(level, x, y, z, state);
        });
    }
}

bool EndDragonFight::_findExitPortalLocation(Level &level) {
    if (level.peekChunkPtr(0, 0) == nullptr)
        return false;

    // a podium generated with the terrain already stands at the origin: find the bottom of its pillar
    const int32_t top = topOfColumn(level, 0, 0);
    bool podium = top > level.getMinY() + PODIUM_BEDROCK_COLUMN;
    for (int32_t y = top; podium && y > top - PODIUM_BEDROCK_COLUMN; --y)
        podium = isBedrock(level.peekBlockPtr(0, y, 0));

    mExitPortalLocation = Vector3i(0, podium ? top - PODIUM_PILLAR_HEIGHT : top + 1, 0);
    return true;
}

void EndDragonFight::_spawnExitPortal(ServerNetworkHandler &owner, Level &level, bool active) {
    (void) owner;
    if (!mExitPortalLocation.has_value() && !_findExitPortalLocation(level))
        return;

    LiveBlockManager manager(level);
    XoroshiroRandom random(0);
    EndPodiumObject podium(active);
    podium.generate(manager, random, mExitPortalLocation->x, mExitPortalLocation->y, mExitPortalLocation->z);
}

void EndDragonFight::_spawnNewGateway(Level &level) {
    std::vector<int32_t> &gateways = _gatewayOrder();
    const Vector3i position = getGatewayPosition(gateways.back());
    gateways.pop_back();
    requestGatewayExit(level, position, true);
}

Vector3i EndDragonFight::findRoughExit(const Level &level, const Vector3i &gateway) {
    const TheEndGenerator *generator = dynamic_cast<const TheEndGenerator *>(&level.getGenerator());
    return EndGatewayExit::findRoughExit(gateway, [generator](int32_t chunkX, int32_t chunkZ) {
        return generator != nullptr && generator->getTerrainTop(chunkX, chunkZ) != 0;
    });
}

void EndDragonFight::requestGatewayExit(const Level &level, const Vector3i &gateway, bool placeGateway) {
    mGatewayRequests.push_back(GatewayRequest{false, gateway, findRoughExit(level, gateway), placeGateway});
}

void EndDragonFight::requestGatewayCheck(const Vector3i &gateway, const Vector3i &exit) {
    mGatewayRequests.push_back(GatewayRequest{true, gateway, exit, false});
}

bool EndDragonFight::_loadGatewayEnds(Level &level, const GatewayRequest &request, bool registerLoaders) {
    bool loaded = true;
    for (const Vector3i &center: {request.mGateway, request.mExit}) {
        for (int32_t chunkX = (center.x - GATEWAY_END_REACH) >> 4; chunkX <= (center.x + GATEWAY_END_REACH) >> 4;
             chunkX++) {
            for (int32_t chunkZ = (center.z - GATEWAY_END_REACH) >> 4; chunkZ <= (center.z + GATEWAY_END_REACH) >> 4;
                 chunkZ++) {
                if (registerLoaders)
                    level.registerChunkLoader(GATEWAY_LOADER_ID, chunkX, chunkZ);

                loaded = level.requestChunkAsync(chunkX, chunkZ) && loaded;
            }
        }
    }

    return loaded;
}

std::vector<std::pair<int32_t, int32_t>> EndDragonFight::_loadGatewayBlockActors(ServerNetworkHandler &owner,
                                                                                  Level &level,
                                                                                  const GatewayRequest &request) {
    std::vector<std::pair<int32_t, int32_t>> loaded;
    if (!level.isStorageOpen())
        return loaded;

    // the game reads the block actors of both ends through its chunk views (sub_14716AEC0): columns without a
    // player keep theirs on disk, so they are read here and written back once the request is done
    BlockActorStore &blockActors = level.getBlockActors();
    for (const Vector3i &center: {request.mGateway, request.mExit}) {
        for (int32_t chunkX = (center.x - GATEWAY_END_REACH) >> 4; chunkX <= (center.x + GATEWAY_END_REACH) >> 4;
             chunkX++) {
            for (int32_t chunkZ = (center.z - GATEWAY_END_REACH) >> 4; chunkZ <= (center.z + GATEWAY_END_REACH) >> 4;
                 chunkZ++) {
                if (blockActors.isChunkLoaded(chunkX, chunkZ))
                    continue;

                owner.loadBlockActorsForChunk(level, chunkX, chunkZ);
                loaded.emplace_back(chunkX, chunkZ);
            }
        }
    }

    return loaded;
}

void EndDragonFight::_releaseGatewayEnds(ServerNetworkHandler &owner, Level &level, const GatewayRequest &request,
                                         const std::vector<std::pair<int32_t, int32_t>> &blockActorChunks) {
    BlockActorStore &blockActors = level.getBlockActors();
    for (const std::pair<int32_t, int32_t> &chunk: blockActorChunks) {
        // a column read without block actors gained none, so there is nothing to write back
        if (blockActors.saveChunk(chunk.first, chunk.second).empty())
            blockActors.unloadChunk(chunk.first, chunk.second);
        else
            owner.saveBlockActorsForChunk(level, chunk.first, chunk.second, true);
    }

    for (const Vector3i &center: {request.mGateway, request.mExit}) {
        for (int32_t chunkX = (center.x - GATEWAY_END_REACH) >> 4; chunkX <= (center.x + GATEWAY_END_REACH) >> 4;
             chunkX++) {
            for (int32_t chunkZ = (center.z - GATEWAY_END_REACH) >> 4; chunkZ <= (center.z + GATEWAY_END_REACH) >> 4;
                 chunkZ++)
                level.unregisterChunkLoader(GATEWAY_LOADER_ID, chunkX, chunkZ);
        }
    }
}

void EndDragonFight::_tickGatewayRequests(ServerNetworkHandler &owner, Level &level) {
    if (mGatewayRequests.empty())
        return;

    // one request at a time, as the game keeps a single pair of chunk views for them (sub_14716C8B0)
    const GatewayRequest request = mGatewayRequests.front();
    const bool registerLoaders = !mGatewayEndsRequested;
    mGatewayEndsRequested = true;
    if (!_loadGatewayEnds(level, request, registerLoaders))
        return;

    const std::vector<std::pair<int32_t, int32_t>> blockActorChunks = _loadGatewayBlockActors(owner, level, request);
    if (request.mCheckOnly) {
        _checkGatewayExit(owner, level, request.mGateway, request.mExit, false);
        _checkGatewayExit(owner, level, request.mExit, request.mGateway, true);
    } else {
        _findGatewayExit(owner, level, request);
    }

    _releaseGatewayEnds(owner, level, request, blockActorChunks);
    mGatewayRequests.pop_front();
    mGatewayEndsRequested = false;
}

EndGatewayBlockActor &EndDragonFight::_placeGateway(Level &level, const Vector3i &position) {
    TheEndDecorationFeature::placeGateway(position.x, position.y, position.z,
                                          [&level](int32_t x, int32_t y, int32_t z, const BlockState &state) {
                                              setIfChanged(level, x, y, z, state);
                                          });
    return level.getBlockActors().getOrCreate<EndGatewayBlockActor>(position);
}

void EndDragonFight::_raiseIsland(Level &level, const Vector3i &position) {
    // the island's random is seeded with the hash of its position (sub_14716ACB0)
    std::mt19937 random(EndGatewayExit::islandSeed(position));
    TheEndDecorationFeature::placeIsland(random, position.x, position.y, position.z,
                                         [&level](int32_t x, int32_t y, int32_t z, const BlockState &state) {
                                             setIfChanged(level, x, y, z, state);
                                         });
}

void EndDragonFight::_findGatewayExit(ServerNetworkHandler &owner, Level &level, const GatewayRequest &request) {
    const EndGatewayExit::BlockReader reader = levelReader(level);
    const int32_t minY = level.getMinY();
    const int32_t maxY = level.getMaxY();

    Vector3i landing = EndGatewayExit::findSpawn(reader, request.mExit, true, 1, false, minY, maxY);
    if (EndGatewayExit::isZero(landing)) {
        landing = Vector3i(request.mExit.x, EndGatewayExit::ISLAND_Y, request.mExit.z);
        _raiseIsland(level, landing);
    }

    Vector3i tallest = EndGatewayExit::findTallestBlock(reader, landing, EndGatewayExit::TALLEST_RADIUS, true, false,
                                                        maxY + 1);
    if (EndGatewayExit::isZero(tallest))
        tallest = landing;
    const Vector3i exit(tallest.x, tallest.y + EndGatewayExit::EXIT_GATEWAY_HEIGHT, tallest.z);

    if (request.mPlaceGateway)
        _placeGateway(level, request.mGateway);
    EndGatewayBlockActor &back = _placeGateway(level, exit);

    EndGatewayBlockActor *origin = level.getBlockActors().find<EndGatewayBlockActor>(request.mGateway);
    if (origin != nullptr) {
        // the entity lands on the tallest block next to the exit gateway, not on its bedrock
        const Vector3i spawn = EndGatewayExit::findSpawn(reader, exit, false, 1, false, minY, maxY);
        Vector3i top = EndGatewayExit::findTallestBlock(reader, spawn, EndGatewayExit::LANDING_RADIUS, false, true,
                                                        maxY + 1);
        if (EndGatewayExit::isZero(top))
            top = spawn;
        origin->setExit(Vector3i(top.x, top.y + 1, top.z));
        BlockActionHandler::broadcastBlockActorData(owner, level, *origin);
    }

    back.setExit(EndGatewayExit::findSpawn(reader, request.mGateway, false, EndGatewayExit::RETURN_SEARCH_CHUNKS,
                                           false, minY, maxY));
    BlockActionHandler::broadcastBlockActorData(owner, level, back);
}

void EndDragonFight::_checkGatewayExit(ServerNetworkHandler &owner, Level &level, const Vector3i &from,
                                       const Vector3i &to, bool fromNearestGateway) {
    const EndGatewayExit::BlockReader reader = levelReader(level);
    if (!EndGatewayExit::isExitBlocked(reader, to))
        return;

    EndGatewayBlockActor *gateway = nullptr;
    Vector3i around = to;
    if (fromNearestGateway) {
        // the gateway leading back is the one closest to the exit, within 32 blocks
        const float range = (float) EndGatewayExit::RETURN_GATEWAY_RANGE;
        float closest = std::numeric_limits<float>::max();
        for (EndGatewayBlockActor *candidate: level.getBlockActors().findAll<EndGatewayBlockActor>()) {
            const Vector3i &position = candidate->getPosition();
            const float dx = (float) from.x + 0.5f - (float) position.x;
            const float dy = (float) from.y + 0.5f - (float) position.y;
            const float dz = (float) from.z + 0.5f - (float) position.z;
            if (std::fabs(dx) > range || std::fabs(dy) > range || std::fabs(dz) > range)
                continue;

            const float distance = dz * dz + (dy * dy + dx * dx);
            if (closest > distance) {
                closest = distance;
                gateway = candidate;
            }
        }

        if (gateway != nullptr)
            around = gateway->getExit();
    } else {
        gateway = level.getBlockActors().find<EndGatewayBlockActor>(from);
    }

    if (gateway == nullptr)
        return;

    const int32_t minY = level.getMinY();
    const int32_t maxY = level.getMaxY();
    Vector3i landing = EndGatewayExit::findSpawn(reader, around, true, 1, false, minY, maxY);
    if (EndGatewayExit::isZero(landing)) {
        _raiseIsland(level, around);
        landing = EndGatewayExit::findSpawn(reader, around, true, 1, true, minY, maxY);
    }

    gateway->setExit(landing);
    BlockActionHandler::broadcastBlockActorData(owner, level, *gateway);
}

void EndDragonFight::onDragonKilled(ServerNetworkHandler &owner, Level &level, const EnderDragon &dragon) {
    if (dragon.getUniqueId() != mDragonUniqueId)
        return;

    _spawnExitPortal(owner, level, true);
    _spawnNewGateway(level);

    if (!mPreviouslyKilled) {
        const int32_t top = topOfColumn(level, 0, 0);
        level.setBlock(Vector3i(0, top + 1, 0), VanillaBlocks::DRAGON_EGG().toBlockState(), true);
    }

    mDragonKilled = true;
    mPreviouslyKilled = true;
}

std::vector<EndCrystalActor *> EndDragonFight::_findCrystals(ServerNetworkHandler &owner, Level &level, float minX,
                                                             float minY, float minZ, float maxX, float maxY,
                                                             float maxZ) {
    const AxisAlignedBB area(minX, minY, minZ, maxX, maxY, maxZ);
    std::vector<EndCrystalActor *> crystals;
    for (auto &entry: owner.getActors()) {
        EndCrystalActor *crystal = dynamic_cast<EndCrystalActor *>(entry.second.get());
        if (crystal == nullptr || crystal->isExpired() || !crystal->isIn(level))
            continue;

        if (ActorPushSystem::boundingBoxOf(*crystal).intersectsWith(area))
            crystals.push_back(crystal);
    }

    return crystals;
}

void EndDragonFight::_updateCrystalCount(ServerNetworkHandler &owner, Level &level) {
    mCrystalsAlive = 0;
    for (const EndSpike &spike: EndSpikes::create((uint32_t) mSeed)) {
        const AxisAlignedBB area = spike.getCrystalArea();
        mCrystalsAlive += (int32_t) _findCrystals(owner, level, area.mMinX, area.mMinY, area.mMinZ, area.mMaxX,
                                                  area.mMaxY, area.mMaxZ).size();
    }
}

void EndDragonFight::_resetSpikeCrystals(ServerNetworkHandler &owner, Level &level) {
    for (const EndSpike &spike: EndSpikes::create((uint32_t) mSeed)) {
        const AxisAlignedBB area = spike.getCrystalArea();
        for (EndCrystalActor *crystal: _findCrystals(owner, level, area.mMinX, area.mMinY, area.mMinZ, area.mMaxX,
                                                     area.mMaxY, area.mMaxZ)) {
            crystal->setInvulnerable(false);
            crystal->setBeamTarget(owner, std::nullopt);
        }
    }
}

void EndDragonFight::tryRespawn(ServerNetworkHandler &owner, Level &level) {
    if (!mDragonKilled || mRespawnStage != RespawnStage::None)
        return;

    if (!mExitPortalLocation.has_value()) {
        _spawnExitPortal(owner, level, true);
        if (!mExitPortalLocation.has_value())
            return;
    }

    // one crystal on each side of the exit portal (sub_1471687D0)
    const Vector3i portal = *mExitPortalLocation;
    const int32_t sides[4][2] = {{0, RESPAWN_CRYSTAL_OFFSET}, {-RESPAWN_CRYSTAL_OFFSET, 0},
                                 {0, -RESPAWN_CRYSTAL_OFFSET}, {RESPAWN_CRYSTAL_OFFSET, 0}};
    std::vector<int64_t> crystals;
    for (const int32_t *side: sides) {
        const float x = (float) (portal.x + side[0]);
        const float z = (float) (portal.z + side[1]);
        const float y = (float) (portal.y + 1);
        const std::vector<EndCrystalActor *> found = _findCrystals(owner, level, x, y, z, x + 1.0f, y + 1.0f,
                                                                   z + 1.0f);
        if (found.empty())
            return;

        for (EndCrystalActor *crystal: found)
            crystals.push_back(crystal->getUniqueId());
    }

    mRespawnCrystals = std::move(crystals);
    mRespawnStage = RespawnStage::Start;
    mRespawnTicks = 0;
    _spawnExitPortal(owner, level, false);
}

void EndDragonFight::_setRespawnBeams(ServerNetworkHandler &owner, const std::optional<Vector3i> &target) {
    for (const int64_t uniqueId: mRespawnCrystals) {
        EndCrystalActor *crystal = dynamic_cast<EndCrystalActor *>(owner.getActor(uniqueId));
        if (crystal != nullptr)
            crystal->setBeamTarget(owner, target);
    }
}

void EndDragonFight::_clearSpikeArea(ServerNetworkHandler &owner, Level &level, const EndSpike &spike) {
    const BlockState air = VanillaBlocks::AIR().toBlockState();
    for (int32_t x = spike.mX - SPIKE_CLEAR_RADIUS; x <= spike.mX + SPIKE_CLEAR_RADIUS; x++) {
        for (int32_t y = spike.mHeight - SPIKE_CLEAR_RADIUS; y <= spike.mHeight + SPIKE_CLEAR_RADIUS; y++) {
            for (int32_t z = spike.mZ - SPIKE_CLEAR_RADIUS; z <= spike.mZ + SPIKE_CLEAR_RADIUS; z++)
                setIfChanged(level, x, y, z, air);
        }
    }

    const Vector3f center((float) spike.mX + 0.5f, (float) spike.mHeight, (float) spike.mZ + 0.5f);
    Explosion explosion(owner, level, center, SPIKE_EXPLOSION_SIZE, nullptr, false);
    explosion.explode();
}

void EndDragonFight::_rebuildSpike(ServerNetworkHandler &owner, Level &level, const EndSpike &spike) {
    EndSpikes::place(spike, true, [&level](int32_t x, int32_t y, int32_t z, const BlockState &state) {
        setIfChanged(level, x, y, z, state);
    });

    // the rebuilt crystal cannot be hurt until the ritual ends and points at the summon point (sub_1471E9580)
    const Vector3i top = spike.getTop();
    setIfChanged(level, top.x, top.y, top.z, EndSpikes::getCrystalBaseState());

    std::mt19937 random((uint32_t) (mSeed ^ (int64_t) owner.getCurrentTick()));
    const float yaw = std::uniform_real_distribution<float>(0.0f, 1.0f)(random) * 360.0f;
    ServerActor *spawned = owner.spawnActor(level, EndCrystalActor::IDENTIFIER, spike.getCrystalPosition(),
                                            [yaw](ServerActor &actor) {
                                                EndCrystalActor &crystal = static_cast<EndCrystalActor &>(actor);
                                                crystal.setRotation(Vector3f(0.0f, yaw, yaw));
                                                crystal.setShowBottom(true);
                                                crystal.setInvulnerable(true);
                                            });
    EndCrystalActor *crystal = dynamic_cast<EndCrystalActor *>(spawned);
    if (crystal != nullptr)
        crystal->setBeamTarget(owner, getSummonPoint());
}

void EndDragonFight::_finishRespawn(ServerNetworkHandler &owner, Level &level) {
    // the ritual ends before the crystals explode, so one caught in the blast is not a broken ritual (sub_14716C8B0)
    mRespawnStage = RespawnStage::None;
    mDragonKilled = false;
    _initializeDragon(owner, level);
    _resetSpikeCrystals(owner, level);

    for (const int64_t uniqueId: mRespawnCrystals) {
        EndCrystalActor *crystal = dynamic_cast<EndCrystalActor *>(owner.getActor(uniqueId));
        if (crystal == nullptr)
            continue;

        crystal->setBeamTarget(owner, std::nullopt);
        Explosion explosion(owner, level, crystal->getPosition(), RESPAWN_CRYSTAL_EXPLOSION_SIZE, crystal, false);
        explosion.explodeWithoutBlocks();
        owner.removeActor(uniqueId);
    }

    mRespawnCrystals.clear();
}

void EndDragonFight::_tickRespawn(ServerNetworkHandler &owner, Level &level) {
    if (mRespawnCrystals.empty()) {
        mRespawnStage = RespawnStage::None;
        mRespawnTicks = 0;
        tryRespawn(owner, level);
        if (mRespawnStage == RespawnStage::None)
            return;
    }

    const int32_t ticks = mRespawnTicks++;
    const std::vector<EndSpike> spikes = EndSpikes::create((uint32_t) mSeed);
    const RespawnStep step = stepRespawn(mRespawnStage, ticks, spikes.size());

    if (step.mBeamToSummonPoint)
        _setRespawnBeams(owner, getSummonPoint());

    if (step.mBeamToSpike >= 0) {
        const EndSpike &spike = spikes[(size_t) step.mBeamToSpike];
        _setRespawnBeams(owner, Vector3i(spike.mX, spike.mHeight + 1, spike.mZ));
    }

    if (step.mClearSpike >= 0)
        _clearSpikeArea(owner, level, spikes[(size_t) step.mClearSpike]);

    if (step.mRebuildSpike >= 0)
        _rebuildSpike(owner, level, spikes[(size_t) step.mRebuildSpike]);

    if (step.mPlaySummonSound)
        owner.playLevelSound(level, SUMMON_SOUND, centerOf(getSummonPoint()), EnderDragon::IDENTIFIER);

    if (step.mSummonDragon)
        _finishRespawn(owner, level);

    mRespawnStage = step.mNextStage;
    if (step.mRestartTimer)
        mRespawnTicks = 0;
}

void EndDragonFight::onCrystalDestroyed(ServerNetworkHandler &owner, Level &level, EndCrystalActor &crystal,
                                        Actor *attacker) {
    if (mRespawnStage != RespawnStage::None) {
        for (const int64_t uniqueId: mRespawnCrystals) {
            if (uniqueId != crystal.getUniqueId())
                continue;

            mRespawnStage = RespawnStage::None;
            mRespawnTicks = 0;
            _resetSpikeCrystals(owner, level);
            _spawnExitPortal(owner, level, true);
            return;
        }
    }

    _updateCrystalCount(owner, level);
    EnderDragon *dragon = dynamic_cast<EnderDragon *>(owner.getActor(mDragonUniqueId));
    if (dragon == nullptr || dragon->isDead())
        return;

    dragon->setCrystalsAlive(mCrystalsAlive);
    dragon->onCrystalDestroyed(owner, crystal, attacker);
}
