#include "World/Level/World.h"

#include <exception>

World::World(uint32_t id, const std::string &name, int viewDistance, int64_t seed)
        : mId(id),
          mOverworld(new Level(name, viewDistance, seed, DimensionType::Overworld)),
          mNether(new Level(name, viewDistance, seed, DimensionType::Nether)),
          mTheEnd(new Level(name, viewDistance, seed, DimensionType::TheEnd)) {
    for (Level *level: getLevels())
        level->setWorldId(id);
}

bool World::open(const std::string &worldsDirectory, size_t workerThreads,
                 const Level::PacketBroadcaster &broadcaster, ServerNetworkHandler *owner) {
    const bool opened = mOverworld->openStorage(worldsDirectory);
    mOverworld->initializeWeather();
    mOverworld->initializeGameRules();

    // An existing world may have a different seed in level.dat. Keep the generators already
    // constructed for the requested seed unless the stored one differs.
    const std::string &name = mOverworld->getName();
    const int viewDistance = mOverworld->getViewDistance();
    if (mNether->getSeed() != mOverworld->getSeed())
        mNether.reset(new Level(name, viewDistance, mOverworld->getSeed(), DimensionType::Nether));
    if (mTheEnd->getSeed() != mOverworld->getSeed())
        mTheEnd.reset(new Level(name, viewDistance, mOverworld->getSeed(), DimensionType::TheEnd));
    mNether->setWorldId(mId);
    mTheEnd->setWorldId(mId);
    mNether->shareGameRulesWith(*mOverworld);
    mTheEnd->shareGameRulesWith(*mOverworld);

    if (mOverworld->isStorageOpen()) {
        mNether->attachStorage(*mOverworld);
        mTheEnd->attachStorage(*mOverworld);
    }

    for (Level *level: getLevels()) {
        level->startWorkers(workerThreads);
        level->setPacketBroadcaster(broadcaster);
        level->setOwner(owner);
    }

    mTickingAreas.load(*this);
    return opened;
}

void World::saveAll(bool wait) {
    for (Level *level: getLevels())
        level->saveAll(wait);
}

void World::close() {
    std::exception_ptr error;
    for (Level *level: {mNether.get(), mTheEnd.get(), mOverworld.get()}) {
        try {
            level->closeStorage();
        } catch (...) {
            if (!error)
                error = std::current_exception();
        }
    }
    if (error)
        std::rethrow_exception(error);
}

Level &World::getLevel(DimensionType dimension) {
    if (dimension == DimensionType::Nether)
        return *mNether;

    if (dimension == DimensionType::TheEnd)
        return *mTheEnd;

    return *mOverworld;
}

std::vector<Level *> World::getLevels() {
    return {mOverworld.get(), mNether.get(), mTheEnd.get()};
}
