#pragma once

#include "World/Level/Dimension/Dimension.h"
#include "World/Level/Dimension/EndDragonFight.h"
#include "World/Level/Level.h"
#include "World/Level/TickingAreaManager.h"

#include <unordered_map>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class ServerNetworkHandler;

// time, weather, game rules, seed and spawn live on the overworld Level, the only one saving level.dat;
// the Nether and the End share its database
class World {
public:
    World(uint32_t id, const std::string &name, int viewDistance, int64_t seed);

    World(const World &) = delete;

    World &operator=(const World &) = delete;

    bool open(const std::string &worldsDirectory, size_t workerThreads, const Level::PacketBroadcaster &broadcaster,
              ServerNetworkHandler *owner);

    void saveAll(bool wait = false);

    // Nether and End first: they write through the overworld database
    void close();

    uint32_t getId() const {
        return mId;
    }

    const std::string &getName() const {
        return mOverworld->getName();
    }

    int64_t getSeed() const {
        return mOverworld->getSeed();
    }

    Level &getOverworld() {
        return *mOverworld;
    }

    // an unknown custom dimension reads as the overworld
    Level &getLevel(DimensionType dimension);

    // opens a registered custom dimension's level: void generation, the overworld's database
    Level &addCustomLevel(DimensionType dimension);

    bool hasLevel(DimensionType dimension) const;

    std::vector<Level *> getLevels();

    TickingAreaManager &getTickingAreas() {
        return mTickingAreas;
    }

    EndDragonFight &getDragonFight() {
        return *mDragonFight;
    }

    std::unordered_set<int64_t> &getActorLoadedChunks(DimensionType dimension) {
        return mActorLoadedChunks[Dimension::toId(dimension)];
    }

    std::vector<int64_t> mActiveCenters;
    int mActiveTickDistance = -1;
    bool mActorPersistencePending = true;
    bool mEnoughPlayersSleeping = false;

private:
    void _saveDragonFight();

    uint32_t mId;
    std::unique_ptr<Level> mOverworld;
    std::unique_ptr<Level> mNether;
    std::unique_ptr<Level> mTheEnd;
    std::unordered_map<int32_t, std::unique_ptr<Level>> mCustomLevels;
    // what open gave: custom levels open later, after the startup event
    size_t mWorkerThreads = 0;
    Level::PacketBroadcaster mBroadcaster;
    ServerNetworkHandler *mOwner = nullptr;
    std::unique_ptr<EndDragonFight> mDragonFight;
    // kept whole: fields the server ignores are written back unchanged
    Tag mTheEndData = Tag::ofCompound();
    TickingAreaManager mTickingAreas;
    std::unordered_map<int32_t, std::unordered_set<int64_t>> mActorLoadedChunks;
};
