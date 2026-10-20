#include "Network/Handler/ServerNetworkHandler.h"

#include "Core/Debug/BedrockLog.h"
#include "Level/AutoCompaction.h"
#include "Level/Generator/Overworld/OverworldGenerator.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Plugin/PluginEvent.h"
#include "Plugin/PluginManager.h"

#include <algorithm>
#include <vector>

World &ServerNetworkHandler::_openWorld(const std::string &name, int64_t seed) {
    World &world = mWorlds.add(name, _getServerViewDistance(), seed);
    const Level::PacketBroadcaster broadcaster = [this](Level &level, const Vector3f &position,
                                                        const Packet &packet) {
        BlockActionHandler::broadcastToViewers(*this, level, position, packet);
    };

    world.open(WorldManager::getWorldsDirectory(), _getChunkWorkerThreadCount(), broadcaster, this);
    AutoCompaction::track(world.getOverworld());
    LOG_INFO(LogAreaID::Server, "Loaded world %s", name.c_str());

    if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_WORLD_LOAD)) {
        PluginEvent loadEvent;
        loadEvent.mType = FALCON_EVENT_WORLD_LOAD;
        loadEvent.mWorldName = name;
        loadEvent.mLevel = &world.getOverworld();
        plugins->dispatch(loadEvent);
    }
    return world;
}

World *ServerNetworkHandler::loadWorld(const std::string &name, bool create, int64_t seed) {
    if (World *loaded = mWorlds.find(name))
        return loaded;

    if (!WorldManager::isValidName(name))
        return nullptr;

    if (!create && !WorldManager::exists(name))
        return nullptr;

    return &_openWorld(name, seed);
}

bool ServerNetworkHandler::unloadWorld(const std::string &name, std::string &error) {
    World *world = mWorlds.find(name);
    if (world == nullptr) {
        error = "falcon.commands.world.notLoaded";
        return false;
    }

    if (world == &mWorlds.getDefault()) {
        error = "falcon.commands.world.unload.default";
        return false;
    }

    for (auto &entry: mPlayers) {
        if (entry.second.getWorldId() == world->getId()) {
            error = "falcon.commands.world.unload.occupied";
            return false;
        }
    }

    if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_WORLD_UNLOAD)) {
        PluginEvent unloadEvent;
        unloadEvent.mType = FALCON_EVENT_WORLD_UNLOAD;
        unloadEvent.mCancellable = true;
        unloadEvent.mWorldName = name;
        unloadEvent.mLevel = &world->getOverworld();
        plugins->dispatch(unloadEvent);
        if (unloadEvent.mCancelled) {
            error = "falcon.commands.world.unload.cancelled";
            return false;
        }
    }

    for (Level *level: world->getLevels()) {
        std::unordered_set<int64_t> &loadedChunks = world->getActorLoadedChunks(level->getDimensionType());
        for (const int64_t column: loadedChunks) {
            const int32_t chunkX = (int32_t) (column >> 32);
            const int32_t chunkZ = (int32_t) (column & 0xffffffff);
            saveActorsForChunk(*level, chunkX, chunkZ, true);
            saveBlockActorsForChunk(*level, chunkX, chunkZ, true);
        }
        loadedChunks.clear();
    }

    std::vector<int64_t> stranded;
    for (auto &entry: mActors) {
        if (entry.second->getWorldId() == world->getId())
            stranded.push_back(entry.first);
    }
    for (const int64_t uniqueId: stranded) {
        auto it = mActors.find(uniqueId);
        if (it == mActors.end())
            continue;

        broadcastActorRemove(*it->second);
        _unregisterActor(uniqueId);
    }

    mItemEntities.erase(std::remove_if(mItemEntities.begin(), mItemEntities.end(),
                                       [world](const std::unique_ptr<ItemActor> &item) {
                                           return item->getWorldId() == world->getId();
                                       }),
                        mItemEntities.end());

    for (auto it = mLingeringClouds.begin(); it != mLingeringClouds.end();) {
        if (it->second.mWorldId == world->getId())
            it = mLingeringClouds.erase(it);
        else
            ++it;
    }

    world->saveAll();
    AutoCompaction::untrack(world->getOverworld());
    world->close();
    LOG_INFO(LogAreaID::Server, "Unloaded world %s", name.c_str());
    mWorlds.remove(world->getId());
    return true;
}
