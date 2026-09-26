#include "Network/Handler/ServerNetworkHandler.h"

#include "Actor/ActorClassRegistry.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Block/BlockActorStore.h"
#include "Core/Debug/BedrockLog.h"
#include "Level/Level.h"
#include "Scripting/Content/CustomContentRegistry.h"

#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

void ServerNetworkHandler::_savePlayerData(const ServerPlayer &player) {
    if (mPlayerData.saveData(player.getName(), player.saveNbt(mLevel.getName())))
        LOG_INFO(LogAreaID::Server, "Saved player data for %s", player.getName().c_str());
    else
        LOG_WARN(LogAreaID::Server, "Could not save player data for %s", player.getName().c_str());
}

void ServerNetworkHandler::_loadPlayerData(ServerPlayer &player) {
    player.setPosition(mLevel.getSpawnPositionForPlayer());
    player.setOp(mOps.isOp(player.getName()));

    Tag data;
    if (!mPlayerData.loadData(player.getName(), data)) {
        player.resetFallDistance();
        return;
    }

    player.loadNbt(data, mCodecContext);
    player.resetFallDistance();

    if (player.getHealth() <= 0.0f) {
        player.setDead(false);
        player.setHealth(player.getMaxHealth());
        player.setPosition(mLevel.getSpawnPositionForPlayer());
        player.resetFallDistance();
    }
}

void ServerNetworkHandler::loadActorsForChunk(Level &level, int32_t chunkX, int32_t chunkZ) {
    const std::vector<Tag> entities = level.loadEntities(chunkX, chunkZ);

    for (const Tag &tag: entities) {
        const std::string identifier = tag.getString("identifier", std::string());
        if (identifier.empty())
            continue;

        const uint64_t runtimeId = allocateRuntimeId();
        const int64_t uniqueId = (int64_t) runtimeId;

        std::unique_ptr<ServerActor> actor;
        if (identifier == FallingBlock::IDENTIFIER)
            actor.reset(new FallingBlock(runtimeId, BlockState()));
        else if (identifier == PrimedTntActor::IDENTIFIER)
            actor.reset(new PrimedTntActor(runtimeId, PrimedTntActor::DEFAULT_FUSE));
        else
            actor = ActorClassRegistry::create(runtimeId, identifier);

        actor->getAttributes() = ActorAttributes::createActorDefaults();

        MobActor *mob = dynamic_cast<MobActor *>(actor.get());
        if (mob != nullptr)
            mob->setMaxHealth(mob->resolveMaxHealth(mProperties.getDifficulty()));

        const CustomActorDefinition *definition = CustomContentRegistry::getInstance().getActorDefinition(identifier);
        if (definition != nullptr) {
            actor->setDefinition(definition);
            actor->setProjectile(definition->mIsProjectile);
        }

        actor->loadNbt(tag);
        actor->initializeProperties();
        if (mob != nullptr)
            mob->getEquipment().loadNbt(tag, mCodecContext);
        actor->setDimension(level.getDimensionType());

        ServerActor *result = actor.get();
        mActors[uniqueId] = std::move(actor);

        broadcastActorSpawn(*result);
        mScriptEngine.onEntityLoad(*result);
    }
}

void ServerNetworkHandler::loadBlockActorsForChunk(Level &level, int32_t chunkX, int32_t chunkZ) {
    level.getBlockActors().loadChunk(chunkX, chunkZ, level.loadBlockEntities(chunkX, chunkZ), mCodecContext);
}

void ServerNetworkHandler::saveBlockActorsForChunk(Level &level, int32_t chunkX, int32_t chunkZ, bool cull) {
    level.saveBlockEntities(chunkX, chunkZ, level.getBlockActors().saveChunk(chunkX, chunkZ));

    if (cull)
        level.getBlockActors().unloadChunk(chunkX, chunkZ);
}

void ServerNetworkHandler::saveActorsForChunk(Level &level, int32_t chunkX, int32_t chunkZ, bool cull) {
    std::vector<Tag> entities;
    std::vector<int64_t> culled;

    for (auto &entry: mActors) {
        ServerActor &actor = *entry.second;
        if (!actor.shouldSave() || actor.getDimension() != level.getDimensionType())
            continue;

        const Vector3f position = actor.getPosition();
        const int32_t actorChunkX = (int32_t) std::floor(position.x) >> 4;
        const int32_t actorChunkZ = (int32_t) std::floor(position.z) >> 4;
        if (actorChunkX != chunkX || actorChunkZ != chunkZ)
            continue;

        entities.push_back(actor.saveNbt());
        if (cull)
            culled.push_back(entry.first);
    }

    level.saveEntities(chunkX, chunkZ, entities);

    for (const int64_t uniqueId: culled) {
        auto it = mActors.find(uniqueId);
        if (it == mActors.end())
            continue;

        mScriptEngine.onEntityRemove(*it->second);
        it = mActors.find(uniqueId);
        if (it == mActors.end())
            continue;

        broadcastActorRemove(*it->second);
        mActors.erase(it);
    }
}

void ServerNetworkHandler::saveAllActors() {
    for (Level *level: getLevels()) {
        for (const int64_t column: mActorLoadedChunks[level->getDimensionId()]) {
            const int32_t chunkX = (int32_t) (column >> 32);
            const int32_t chunkZ = (int32_t) (column & 0xffffffff);
            saveActorsForChunk(*level, chunkX, chunkZ, false);
            saveBlockActorsForChunk(*level, chunkX, chunkZ, false);
        }
    }
}

void ServerNetworkHandler::autoSave() {
    if (!mLevel.isStorageOpen())
        return;

    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    for (auto &entry: mPlayers) {
        const ServerPlayer &player = entry.second;
        if (player.isSpawned() && !player.getName().empty())
            _savePlayerData(player);
    }

    saveWorldDynamicProperties();
    saveAllActors();

    mLevel.saveAll();
    if (mNetherLevel != nullptr)
        mNetherLevel->saveAll();
    if (mTheEndLevel != nullptr)
        mTheEndLevel->saveAll();

    const long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
    LOG_INFO(LogAreaID::Server, "Auto-saved the world in %lld ms", elapsed);
}

bool ServerNetworkHandler::syncActorPersistence(Level &level, const std::vector<int64_t> &activeColumns) {
    if (!level.isStorageOpen())
        return false;

    std::unordered_set<int64_t> &loadedChunks = mActorLoadedChunks[level.getDimensionId()];
    const std::unordered_set<int64_t> active(activeColumns.begin(), activeColumns.end());
    bool deferred = false;

    for (const int64_t column: active) {
        if (loadedChunks.count(column) != 0)
            continue;

        const int32_t chunkX = (int32_t) (column >> 32);
        const int32_t chunkZ = (int32_t) (column & 0xffffffff);
        if (!level.isChunkResident(chunkX, chunkZ)) {
            deferred = true;
            continue;
        }

        loadActorsForChunk(level, chunkX, chunkZ);
        loadBlockActorsForChunk(level, chunkX, chunkZ);
        loadedChunks.insert(column);
    }

    std::vector<int64_t> unloaded;
    for (const int64_t column: loadedChunks) {
        if (active.count(column) == 0)
            unloaded.push_back(column);
    }

    for (const int64_t column: unloaded) {
        const int32_t chunkX = (int32_t) (column >> 32);
        const int32_t chunkZ = (int32_t) (column & 0xffffffff);
        saveActorsForChunk(level, chunkX, chunkZ, true);
        saveBlockActorsForChunk(level, chunkX, chunkZ, true);
        loadedChunks.erase(column);
    }

    return deferred;
}
