#include "Falcon/Plugin/PluginServerApi.h"

#include "Falcon/Plugin/PluginApiHelpers.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/Binding/ScriptRaycast.h"
#include "Scripting/ScriptEngine.h"
#include "Scripting/ScriptStructureRegistry.h"
#include "Server/PropertiesSettings.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/GameRules.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/Structure/McStructure.h"
#include "World/Level/TickingAreaManager.h"
#include "World/Level/World.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace PluginApiHelpers;

namespace {
    constexpr int32_t CHUNK_SIZE = 16;
    constexpr uint32_t MAX_DIFFICULTY = FALCON_DIFFICULTY_HARD;

    Vector3f toVector3f(const FalconVec3 &value) {
        return Vector3f((float) value.x, (float) value.y, (float) value.z);
    }

    GameRules &rulesOf(FalconLevel *handle) {
        return owner().getWorldOf(*level(handle)).getOverworld().getGameRules();
    }

    uint32_t levelGameRuleCount(FalconLevel *handle) {
        return (uint32_t) rulesOf(handle).getNames().size();
    }

    const char *levelGameRuleName(FalconLevel *handle, uint32_t index) {
        const std::vector<std::string> names = rulesOf(handle).getNames();
        return index < names.size() ? hold(names[index]) : nullptr;
    }

    const char *levelGameRule(FalconLevel *handle, const char *name) {
        const GameRules::Rule *rule = name == nullptr ? nullptr : rulesOf(handle).find(name);
        if (rule == nullptr)
            return nullptr;
        if (rule->mType == GameRuleData::Type::Bool)
            return rule->mBoolValue ? "true" : "false";
        return hold(std::to_string(rule->mIntValue));
    }

    int levelSetGameRule(FalconLevel *handle, const char *name, const char *value) {
        if (name == nullptr || value == nullptr)
            return 0;
        return owner().changeGameRule(owner().getWorldOf(*level(handle)), name, value) ? 1 : 0;
    }

    FalconDifficulty serverDifficulty() {
        return (FalconDifficulty) owner().getProperties().getDifficulty();
    }

    int serverSetDifficulty(FalconDifficulty difficulty) {
        if (difficulty > MAX_DIFFICULTY)
            return 0;
        owner().changeDifficulty((Difficulty) difficulty);
        return 1;
    }

    // biomes are known by name to the generator data, without the namespace the game shows
    const char *levelBiome(FalconLevel *handle, FalconBlockPos position) {
        Level &target = *level(handle);
        if (position.y < target.getMinY() || position.y > target.getMaxY())
            return nullptr;

        LevelChunk *chunk = target.peekChunkPtr(position.x >> 4, position.z >> 4);
        const int32_t id = chunk != nullptr ? (int32_t) chunk->getBiomeAt(position.x & 15, position.y, position.z & 15)
                                            : target.pickBiome(position.x, position.y, position.z);
        BiomeChunkGenDataRegistry::initialize();
        for (const std::string &name: BiomeChunkGenDataRegistry::getBiomeNames()) {
            if (BiomeChunkGenDataRegistry::getBiomeId(name) == id)
                return hold(name.find(':') == std::string::npos ? "minecraft:" + name : name);
        }
        return nullptr;
    }

    bool unitDirection(const FalconVec3 &direction, Vector3f &out) {
        const double length = std::sqrt(direction.x * direction.x + direction.y * direction.y
                                        + direction.z * direction.z);
        if (!std::isfinite(length) || length <= 0.0)
            return false;
        out = Vector3f((float) (direction.x / length), (float) (direction.y / length), (float) (direction.z / length));
        return true;
    }

    float rayLength(double maxDistance) {
        if (!std::isfinite(maxDistance) || maxDistance <= 0.0)
            return 0.0f;
        return (float) std::min(maxDistance, (double) ScriptRaycast::MAX_DISTANCE);
    }

    FalconBlockFace faceOf(const char *name) {
        static const char *const NAMES[] = {"Down", "Up", "North", "South", "West", "East"};
        for (uint32_t index = 0; index < 6; ++index) {
            if (std::strcmp(NAMES[index], name) == 0)
                return index;
        }
        return FALCON_FACE_UP;
    }

    int levelRaycastBlock(FalconLevel *handle, FalconVec3 origin, FalconVec3 direction, double maxDistance,
                          int includeLiquids, int includePassable, FalconBlockHit *out) {
        Vector3f unit;
        const float length = rayLength(maxDistance);
        if (out == nullptr || length <= 0.0f || !unitDirection(direction, unit))
            return 0;

        ScriptRaycast::BlockOptions options;
        options.mMaxDistance = length;
        options.mIncludeLiquids = includeLiquids != 0;
        options.mIncludePassable = includePassable != 0;
        ScriptRaycast::BlockHit hit;
        if (!ScriptRaycast::castBlocks(*level(handle), toVector3f(origin), unit, options, hit))
            return 0;

        out->block = FalconBlockPos{hit.mBlock.x, hit.mBlock.y, hit.mBlock.z};
        out->face = faceOf(hit.mFace);
        out->position = FalconVec3{(double) hit.mBlock.x + hit.mFaceLocation.x,
                                   (double) hit.mBlock.y + hit.mFaceLocation.y,
                                   (double) hit.mBlock.z + hit.mFaceLocation.z};
        out->distance = hit.mDistance;
        return 1;
    }

    // every live entity and spawned player of the level
    template<typename Visitor>
    void forEachEntity(const Level &target, Visitor &&visit) {
        for (auto &entry: owner().getActors()) {
            if (entry.second->isAlive() && entry.second->isIn(target))
                visit(static_cast<Actor &>(*entry.second));
        }
        for (auto &entry: owner().getPlayers()) {
            if (entry.second.isSpawned() && entry.second.isIn(target))
                visit(static_cast<Actor &>(entry.second));
        }
    }

    // blocks stop the ray, like an entity ray of scripts
    FalconEntity *levelRaycastEntity(FalconLevel *handle, FalconVec3 origin, FalconVec3 direction, double maxDistance,
                                     FalconEntity *except, double *distance) {
        Vector3f unit;
        float length = rayLength(maxDistance);
        if (length <= 0.0f || !unitDirection(direction, unit))
            return nullptr;

        Level &target = *level(handle);
        const Vector3f from = toVector3f(origin);
        ScriptRaycast::BlockOptions options;
        options.mMaxDistance = length;
        ScriptRaycast::BlockHit wall;
        if (ScriptRaycast::castBlocks(target, from, unit, options, wall))
            length = std::min(length, wall.mDistance);

        const Actor *skipped = except == nullptr ? nullptr : entity(except);
        Actor *nearest = nullptr;
        float best = 0.0f;
        forEachEntity(target, [&](Actor &candidate) {
            if (&candidate == skipped)
                return;
            const ActorSize size = ScriptRaycast::actorSize(candidate);
            const Vector3f &at = candidate.getPosition();
            const float half = size.mWidth / 2.0f;
            const float entered = ScriptRaycast::intersectBox(from, unit, Vector3f(at.x - half, at.y, at.z - half),
                                                              Vector3f(at.x + half, at.y + size.mHeight, at.z + half));
            if (entered < 0.0f || entered > length || (nearest != nullptr && entered >= best))
                return;
            nearest = &candidate;
            best = entered;
        });

        if (nearest != nullptr && distance != nullptr)
            *distance = best;
        return toHandle(nearest);
    }

    uint32_t levelEntitiesInBox(FalconLevel *handle, FalconVec3 from, FalconVec3 to, FalconEntity **entities,
                                uint32_t capacity) {
        const Vector3f low((float) std::min(from.x, to.x), (float) std::min(from.y, to.y),
                           (float) std::min(from.z, to.z));
        const Vector3f high((float) std::max(from.x, to.x), (float) std::max(from.y, to.y),
                            (float) std::max(from.z, to.z));
        uint32_t count = 0;
        forEachEntity(*level(handle), [&](Actor &candidate) {
            const ActorSize size = ScriptRaycast::actorSize(candidate);
            const Vector3f &at = candidate.getPosition();
            const float half = size.mWidth / 2.0f;
            if (at.x + half < low.x || at.x - half > high.x || at.y + size.mHeight < low.y || at.y > high.y
                || at.z + half < low.z || at.z - half > high.z)
                return;
            if (entities != nullptr && count < capacity)
                entities[count] = toHandle(&candidate);
            count++;
        });
        return count;
    }

    int levelPlaceStructure(FalconLevel *handle, const char *structure, FalconBlockPos position, uint32_t rotation,
                            FalconMirror mirror, int includeEntities, float integrity) {
        if (structure == nullptr || !ScriptStructureRegistry::isValidIdentifier(structure) || rotation > 3
            || mirror > FALCON_MIRROR_XZ || !(integrity >= 0.0f && integrity <= 1.0f))
            return 0;

        std::shared_ptr<McStructure> found =
                owner().getScriptEngine().structures().find(ScriptStructureRegistry::normalize(structure));
        if (found == nullptr)
            return 0;

        McStructure::PlaceOptions options;
        options.mRotation = (int32_t) rotation;
        options.mMirror = (McStructure::Mirror) mirror;
        options.mEntities = includeEntities != 0;
        options.mIntegrity = integrity;

        Level &target = *level(handle);
        const Vector3i size = found->rotatedSize(options.mRotation);
        if (position.y < target.getMinY() || position.y + size.y - 1 > target.getMaxY())
            return 0;

        found->place(owner(), target, Vector3i(position.x, position.y, position.z), options);
        return 1;
    }

    int levelAddTickingArea(FalconLevel *handle, const char *name, FalconBlockPos from, FalconBlockPos to,
                            int preload) {
        if (name == nullptr || *name == '\0')
            return 0;

        Level &target = *level(handle);
        World &world = owner().getWorldOf(target);
        TickingAreaManager &areas = world.getTickingAreas();
        TickingArea area = TickingArea::box(Vector3i(from.x, from.y, from.z), Vector3i(to.x, to.y, to.z));
        const int64_t chunks = (int64_t) ((area.mMaxX - area.mMinX + 1) / CHUNK_SIZE)
                               * ((area.mMaxZ - area.mMinZ + 1) / CHUNK_SIZE);
        if (chunks > TickingAreaManager::MAX_CHUNKS || areas.getAreas().size() >= TickingAreaManager::MAX_AREAS
            || areas.hasName(name))
            return 0;

        area.mName = name;
        area.mDimension = target.getDimensionType();
        area.mPreload = preload != 0;
        areas.add(world, area);
        return 1;
    }

    int levelRemoveTickingArea(FalconLevel *handle, const char *name) {
        if (name == nullptr)
            return 0;

        Level &target = *level(handle);
        const DimensionType dimension = target.getDimensionType();
        World &world = owner().getWorldOf(target);
        const std::string wanted = name;
        const std::vector<TickingArea> removed = world.getTickingAreas().remove(world, [&](const TickingArea &area) {
            return area.mDimension == dimension && area.mName == wanted;
        });
        return removed.empty() ? 0 : 1;
    }

    std::vector<const TickingArea *> areasOf(FalconLevel *handle) {
        Level &target = *level(handle);
        std::vector<const TickingArea *> found;
        for (const TickingArea &area: owner().getWorldOf(target).getTickingAreas().getAreas()) {
            if (area.mDimension == target.getDimensionType())
                found.push_back(&area);
        }
        return found;
    }

    uint32_t levelTickingAreaCount(FalconLevel *handle) {
        return (uint32_t) areasOf(handle).size();
    }

    const char *levelTickingAreaName(FalconLevel *handle, uint32_t index) {
        const std::vector<const TickingArea *> areas = areasOf(handle);
        return index < areas.size() ? hold(areas[index]->mName) : nullptr;
    }
}

void PluginServerApi::fillWorldQueries(FalconServerApi &api) {
    api.levelGameRuleCount = &levelGameRuleCount;
    api.levelGameRuleName = &levelGameRuleName;
    api.levelGameRule = &levelGameRule;
    api.levelSetGameRule = &levelSetGameRule;
    api.serverDifficulty = &serverDifficulty;
    api.serverSetDifficulty = &serverSetDifficulty;
    api.levelBiome = &levelBiome;
    api.levelRaycastBlock = &levelRaycastBlock;
    api.levelRaycastEntity = &levelRaycastEntity;
    api.levelEntitiesInBox = &levelEntitiesInBox;
    api.levelPlaceStructure = &levelPlaceStructure;
    api.levelAddTickingArea = &levelAddTickingArea;
    api.levelRemoveTickingArea = &levelRemoveTickingArea;
    api.levelTickingAreaCount = &levelTickingAreaCount;
    api.levelTickingAreaName = &levelTickingAreaName;
}
