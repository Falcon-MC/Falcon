#include "World/Level/World.h"

#include <exception>

World::World(uint32_t id, const std::string &name, int viewDistance, int64_t seed)
        : mId(id), mOverworld(new Level(name, viewDistance, seed, DimensionType::Overworld)),
          mNether(new Level(name, viewDistance, seed, DimensionType::Nether)),
          mTheEnd(new Level(name, viewDistance, seed, DimensionType::TheEnd)), mDragonFight(new EndDragonFight(seed)) {
    for (Level *level: getLevels())
        level->setWorldId(id);
}

bool World::open(const std::string &worldsDirectory, size_t workerThreads, const Level::PacketBroadcaster &broadcaster,
                 ServerNetworkHandler *owner) {
    const bool opened = mOverworld->openStorage(worldsDirectory);
    mOverworld->initializeWeather();
    mOverworld->initializeGameRules();

    // level.dat may hold another seed: rebuild the generators only when it differs
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

    mWorkerThreads = workerThreads;
    mBroadcaster = broadcaster;
    mOwner = owner;
    for (Level *level: getLevels()) {
        level->startWorkers(workerThreads);
        level->setPacketBroadcaster(broadcaster);
        level->setOwner(owner);
    }

    mDragonFight.reset(new EndDragonFight(mOverworld->getSeed()));
    Tag endData;
    if (mTheEnd->loadSavedData(EndDragonFight::SAVED_DATA_KEY, endData)) {
        mTheEndData = endData;
        mDragonFight->load(mTheEndData);
    }

    mTickingAreas.load(*this);
    return opened;
}

void World::saveAll(bool wait) {
    for (Level *level: getLevels())
        level->saveAll(wait);

    _saveDragonFight();
}

void World::_saveDragonFight() {
    if (!mTheEnd->isStorageOpen())
        return;

    mDragonFight->save(mTheEndData);
    mTheEnd->saveSavedData(EndDragonFight::SAVED_DATA_KEY, mTheEndData);
}

void World::close() {
    _saveDragonFight();

    std::vector<Level *> closing = {mNether.get(), mTheEnd.get()};
    for (auto &entry: mCustomLevels)
        closing.push_back(entry.second.get());
    closing.push_back(mOverworld.get());

    std::exception_ptr error;
    for (Level *level: closing) {
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

    const auto custom = mCustomLevels.find(Dimension::toId(dimension));
    if (custom != mCustomLevels.end())
        return *custom->second;

    return *mOverworld;
}

bool World::hasLevel(DimensionType dimension) const {
    return !Dimension::isCustom(dimension) || mCustomLevels.count(Dimension::toId(dimension)) != 0;
}

Level &World::addCustomLevel(DimensionType dimension) {
    std::unique_ptr<Level> &slot = mCustomLevels[Dimension::toId(dimension)];
    if (slot != nullptr)
        return *slot;

    slot.reset(new Level(mOverworld->getName(), mOverworld->getViewDistance(), mOverworld->getSeed(), dimension));
    slot->setWorldId(mId);
    slot->shareGameRulesWith(*mOverworld);
    if (mOverworld->isStorageOpen())
        slot->attachStorage(*mOverworld);
    if (mWorkerThreads != 0)
        slot->startWorkers(mWorkerThreads);
    if (mBroadcaster)
        slot->setPacketBroadcaster(mBroadcaster);
    slot->setOwner(mOwner);
    return *slot;
}

std::vector<Level *> World::getLevels() {
    std::vector<Level *> levels = {mOverworld.get(), mNether.get(), mTheEnd.get()};
    for (auto &entry: mCustomLevels)
        levels.push_back(entry.second.get());
    return levels;
}
