#pragma once

#include "Level/Dimension.h"
#include "Level/Level.h"
#include "Level/TickingAreaManager.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class ServerNetworkHandler;

/**
 * One world folder (`worlds/<name>`) with its overworld, Nether and End. Time, weather, game rules, seed and
 * spawn stay on the overworld Level, which is the only dimension that saves level.dat; the Nether and the End
 * share its database.
 */
class World {
public:
    World(uint32_t id, const std::string &name, int viewDistance, int64_t seed);

    World(const World &) = delete;

    World &operator=(const World &) = delete;

    bool open(const std::string &worldsDirectory, size_t workerThreads, const Level::PacketBroadcaster &broadcaster,
              ServerNetworkHandler *owner);

    void saveAll();

    /** Closes the Nether and the End before the overworld, since they write through its database. */
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

    Level &getLevel(DimensionType dimension);

    std::vector<Level *> getLevels();

    TickingAreaManager &getTickingAreas() {
        return mTickingAreas;
    }

    std::unordered_set<int64_t> &getActorLoadedChunks(DimensionType dimension) {
        return mActorLoadedChunks[Dimension::toId(dimension)];
    }

    std::vector<int64_t> mActiveCenters;
    int mActiveTickDistance = -1;
    bool mActorPersistencePending = true;
    int32_t mSleepTicks = 0;

private:
    uint32_t mId;
    std::unique_ptr<Level> mOverworld;
    std::unique_ptr<Level> mNether;
    std::unique_ptr<Level> mTheEnd;
    TickingAreaManager mTickingAreas;
    std::array<std::unordered_set<int64_t>, Dimension::DIMENSION_COUNT> mActorLoadedChunks;
};
