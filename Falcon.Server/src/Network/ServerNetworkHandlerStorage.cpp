#include "Network/ServerNetworkHandler.h"

#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "Core/Debug/BedrockLog.h"
#include "World/Level/Level.h"
#include "Network/Handler/ItemActorHandler.h"
#include "World/Level/Block/Systems/PistonSystem.h"
#include "Scripting/Content/CustomContentRegistry.h"

#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
    // packed (x << 32) | z; false when not saved with `level`; both the chunk and level-wide saves sort by it
    bool savedActorColumn(const ServerActor &actor, const Level &level, int64_t &column) {
        if (!actor.shouldSave() || !actor.isIn(level))
            return false;

        const Vector3f position = actor.getPosition();
        column = ((int64_t) ((int32_t) std::floor(position.x) >> 4) << 32)
                 | (uint32_t) ((int32_t) std::floor(position.z) >> 4);
        return true;
    }
}

void ServerNetworkHandler::_savePlayerData(const ServerPlayer &player, bool checked) {
    // loaded in sendStartGame: before it the player is blank, and would overwrite its own save
    if (player.getLoginState() < ServerPlayer::LoginState::StartGameSent)
        return;

    Tag data = player.saveNbt(getWorldFor(player).getName());
    if (!player.getXuid().empty())
        data.putString(PlayerDataProvider::TAG_OWNER_XUID, player.getXuid());

    if (mPlayerData.saveData(player.getName(), data))
        LOG_INFO(LogAreaID::Server, "Saved player data for %s", player.getName().c_str());
    else {
        LOG_WARN(LogAreaID::Server, "Could not save player data for %s", player.getName().c_str());
        if (checked)
            throw std::runtime_error("Could not save player: " + player.getName());
    }
}

void ServerNetworkHandler::_loadPlayerData(ServerPlayer &player) {
    World &defaultWorld = mWorlds.getDefault();
    player.moveToLevel(defaultWorld.getOverworld());
    player.setPosition(defaultWorld.getOverworld().getSpawnPositionForPlayer());
    mOps.bindXuid(player.getName(), player.getXuid());
    player.setOp(mOps.isOp(player.getName(), player.getXuid()));

    player.setUniqueId(allocateActorUniqueId());

    Tag data;
    if (!mPlayerData.loadData(player.getName(), data)) {
        player.resetFallDistance();
        return;
    }

    player.loadNbt(data, mCodecContext);
    player.resetFallDistance();

    World *savedWorld = mWorlds.find(data.getString(ServerPlayer::TAG_LEVEL, std::string()));
    if (savedWorld != nullptr) {
        player.moveToLevel(savedWorld->getLevel(player.getDimension()));
    } else {
        player.moveToLevel(defaultWorld.getOverworld());
        player.setPosition(defaultWorld.getOverworld().getSpawnPositionForPlayer());
    }

    if (player.getHealth() <= 0.0f) {
        player.setDead(false);
        player.setHealth(player.getMaxHealth());
        Level &overworld = getWorldFor(player).getOverworld();
        player.moveToLevel(overworld);
        player.setPosition(overworld.getSpawnPositionForPlayer());
        player.resetFallDistance();
    }
}

void ServerNetworkHandler::loadActorsForChunk(Level &level, int32_t chunkX, int32_t chunkZ) {
    spawnStoredActors(level, level.loadEntities(chunkX, chunkZ));
}

void ServerNetworkHandler::spawnStoredActors(Level &level, const std::vector<Tag> &entities) {
    for (const Tag &tag: entities) {
        const std::string identifier = tag.getString("identifier", std::string());
        if (identifier.empty())
            continue;

        if (identifier == ItemActor::IDENTIFIER) {
            ItemActorHandler::restoreItem(*this, level, tag);
            continue;
        }

        const auto loaded = tag.contains("UniqueID") ? mActors.find(tag.getLong("UniqueID")) : mActors.end();
        if (loaded != mActors.end() && loaded->second->isIn(level))
            continue;

        // unique ids are unique only per world database: a clashing actor from another world gets a fresh id;
        // generated mobs have none yet: chunk workers can't allocate one
        const bool idTaken = loaded != mActors.end() || !tag.contains("UniqueID");

        const uint64_t runtimeId = allocateRuntimeId();

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
        if (mob != nullptr && mProperties.getDifficulty() == Difficulty::Peaceful && mob->shouldRemoveInPeaceful()) {
            level.eraseEntity(actor->getUniqueId());
            continue;
        }

        if (idTaken)
            actor->setUniqueId(allocateActorUniqueId());
        actor->initializeProperties();
        if (mob != nullptr)
            mob->getEquipment().loadNbt(tag, mCodecContext);
        else
            actor->loadEquipment(tag, mCodecContext);
        actor->moveToLevel(level);

        ServerActor *result = _registerActor(std::move(actor));

        broadcastActorSpawn(*result);
        if (MobActor *loaded = dynamic_cast<MobActor *>(result))
            loaded->upgradeDefinition(*this, true);
        mScriptEngine.onEntitySpawn(*result, "Loaded");
        mScriptEngine.onEntityLoad(*result);
    }

    _resolvePendingRides();
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

    const int64_t target = ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
    for (auto &entry: mActors) {
        ServerActor &actor = *entry.second;
        int64_t column = 0;
        if (!savedActorColumn(actor, level, column) || column != target)
            continue;

        entities.push_back(actor.saveNbt());
        if (cull)
            culled.push_back(entry.first);
    }

    ItemActorHandler::saveItemsInChunk(*this, level, chunkX, chunkZ, entities, cull);
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
        _unregisterActor(uniqueId);
    }
}

void ServerNetworkHandler::saveAllActors() {
    for (Level *level: getLevels()) {
        PistonSystem::finishMoves(*this, *level);
        const std::unordered_set<int64_t> &columns = getWorldOf(*level).getActorLoadedChunks(level->getDimensionType());

        // one pass, in saveActorsForChunk() order; asking each loaded chunk walks every actor once per chunk
        std::unordered_map<int64_t, std::vector<Tag>> byChunk;
        for (auto &entry: mActors) {
            ServerActor &actor = *entry.second;
            int64_t column = 0;
            if (savedActorColumn(actor, *level, column) && columns.count(column) != 0)
                byChunk[column].push_back(actor.saveNbt());
        }

        for (const int64_t column: columns) {
            const int32_t chunkX = (int32_t) (column >> 32);
            const int32_t chunkZ = (int32_t) (column & 0xffffffff);

            std::vector<Tag> entities;
            const auto found = byChunk.find(column);
            if (found != byChunk.end())
                entities = std::move(found->second);

            ItemActorHandler::saveItemsInChunk(*this, *level, chunkX, chunkZ, entities, false);
            level->saveEntities(chunkX, chunkZ, entities);
            saveBlockActorsForChunk(*level, chunkX, chunkZ, false);
        }

        saveStrayActors(*level, columns);
    }
}

// one walked out of the loaded columns keeps ticking: the column it left is written without it
void ServerNetworkHandler::saveStrayActors(Level &level, const std::unordered_set<int64_t> &loadedColumns) {
    std::unordered_map<int64_t, std::vector<Tag>> byChunk;
    for (auto &entry: mActors) {
        int64_t column = 0;
        if (savedActorColumn(*entry.second, level, column) && loadedColumns.count(column) == 0)
            byChunk[column].push_back(entry.second->saveNbt());
    }

    for (const std::unique_ptr<ItemActor> &item: mItemEntities) {
        if (item->isRemoved() || !item->isIn(level))
            continue;

        const Vector3f &position = item->getPosition();
        const int64_t column = ((int64_t) ((int32_t) std::floor(position.x) >> 4) << 32)
                               | (uint32_t) ((int32_t) std::floor(position.z) >> 4);
        if (loadedColumns.count(column) == 0)
            byChunk[column].push_back(item->saveNbt());
    }

    for (const auto &entry: byChunk)
        level.appendEntities((int32_t) (entry.first >> 32), (int32_t) (entry.first & 0xffffffff), entry.second);
}

void ServerNetworkHandler::autoSave(bool wait) {
    if (!getLevel().isStorageOpen())
        return;

    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::vector<std::pair<Level *, uint64_t>> failures;
    if (wait) {
        for (Level *level: getLevels())
            failures.emplace_back(level, level->getSaveFailureCount());
    }

    for (auto &entry: mPlayers) {
        const ServerPlayer &player = entry.second;
        if (player.isSpawned() && !player.getName().empty() && !player.isSimulated())
            _savePlayerData(player, wait);
    }

    saveWorldDynamicProperties(wait);
    saveScoreboard(wait);
    mScriptEngine.saveWorldClocks();
    saveAllActors();

    for (World *world: mWorlds.getWorlds())
        world->saveAll(wait);
    for (const auto &entry: failures) {
        if (entry.first->getSaveFailureCount() != entry.second)
            throw std::runtime_error("Could not save level: " + entry.first->getName());
    }

    const long long elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    LOG_INFO(LogAreaID::Server, "Auto-saved the world in %lld ms", elapsed);
}

bool ServerNetworkHandler::syncActorPersistence(Level &level, const std::vector<int64_t> &activeColumns) {
    if (!level.isStorageOpen())
        return false;

    std::unordered_set<int64_t> &loadedChunks = getWorldOf(level).getActorLoadedChunks(level.getDimensionType());
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
    if (!unloaded.empty()) {
        const std::unordered_set<int64_t> leaving(unloaded.begin(), unloaded.end());
        PistonSystem::finishMoves(*this, level, &leaving);
    }

    for (const int64_t column: unloaded) {
        const int32_t chunkX = (int32_t) (column >> 32);
        const int32_t chunkZ = (int32_t) (column & 0xffffffff);
        saveActorsForChunk(level, chunkX, chunkZ, true);
        saveBlockActorsForChunk(level, chunkX, chunkZ, true);
        loadedChunks.erase(column);
    }
    if (!unloaded.empty())
        saveStrayActors(level, loadedChunks);

    return deferred;
}
