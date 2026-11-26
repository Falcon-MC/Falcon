#include "World/Level/Block/Circuit/CircuitSystem.h"

BaseCircuitComponent *CircuitSystem::_create(const Vector3i &position, uint8_t direction,
                                             std::unique_ptr<BaseCircuitComponent> component) {
    component->setDirection(direction);
    if (mLockGraph)
        return nullptr;
    return mSceneGraph.add(position, std::move(component));
}

void CircuitSystem::removeComponents(const Vector3i &position) {
    if (mLockGraph)
        return;
    mSceneGraph.remove(position, mSceneGraph.getComponent(position));
}

BaseCircuitComponent *CircuitSystem::getComponent(const Vector3i &position) const {
    BaseCircuitComponent *component = mSceneGraph.getComponent(position);
    return component != nullptr && !component->isRemoved() ? component : nullptr;
}

BaseCircuitComponent *CircuitSystem::findComponent(const Vector3i &position) const {
    BaseCircuitComponent *pending = mSceneGraph.getPendingComponent(position);
    return pending != nullptr ? pending : getComponent(position);
}

int CircuitSystem::getStrength(const Vector3i &position) const {
    const BaseCircuitComponent *component = getComponent(position);
    return component == nullptr ? -1 : component->getStrength();
}

void CircuitSystem::setStrength(const Vector3i &position, int strength) {
    BaseCircuitComponent *component = findComponent(position);
    if (component != nullptr)
        component->setStrength(strength);
}

bool CircuitSystem::tickRedstone(CircuitBlockSource *region) {
    int counter = mTickCounter;
    bool evaluated = false;
    if (counter >= TICK_RATE && region != nullptr) {
        evaluate(region);
        counter = 0;
        evaluated = true;
    }
    mTickCounter = counter + 1;
    return evaluated;
}

void CircuitSystem::_evaluateComponents(bool producers) {
    const uint8_t mask = producers ? CircuitFlags::PRODUCER_PASS : CircuitFlags::CONSUMER_PASS;
    for (auto &chunk: mSceneGraph.getChunkLists()) {
        if (!chunk.second.mShouldEvaluate)
            continue;
        for (const CircuitSceneGraph::ChunkEntry &entry: chunk.second.mEntries) {
            BaseCircuitComponent *component = entry.mComponent;
            if ((entry.mFlags & mask) == 0 || component == nullptr || component->isRemoved())
                continue;
            const CircuitComponentType base = component->getBaseType();
            const bool producer = base == CircuitComponentType::Producer || base == CircuitComponentType::Capacitor;
            if (producer != producers)
                continue;
            if (component->evaluate(*this, entry.mPos))
                component->setNeedsUpdate(true);
        }
    }
}

// capacitors read what their sources held before this evaluation: each one delays by an evaluation
void CircuitSystem::evaluate(CircuitBlockSource *region) {
    for (auto &chunk: mSceneGraph.getChunkLists()) {
        const int32_t chunkX = (int32_t) (chunk.first >> 32);
        const int32_t chunkZ = (int32_t) (uint32_t) chunk.first;
        chunk.second.mShouldEvaluate = region == nullptr
                                       || (region->isLoaded(Vector3i(chunkX * 16, 0, chunkZ * 16))
                                           && !_isNearUnloaded(chunk.first));
    }

    for (auto &chunk: mSceneGraph.getChunkLists()) {
        if (!chunk.second.mShouldEvaluate)
            continue;
        for (const CircuitSceneGraph::ChunkEntry &entry: chunk.second.mEntries) {
            if ((entry.mFlags & CircuitFlags::CACHE_VALUES) != 0 && entry.mComponent != nullptr
                && !entry.mComponent->isRemoved())
                entry.mComponent->cacheValues(*this, entry.mPos);
        }
    }

    _evaluateComponents(true);
    _evaluateComponents(false);

    for (auto &chunk: mSceneGraph.getChunkLists()) {
        if (!chunk.second.mShouldEvaluate)
            continue;
        for (const CircuitSceneGraph::ChunkEntry &entry: chunk.second.mEntries) {
            if ((entry.mFlags & CircuitFlags::CHECK_LOCK) != 0 && entry.mComponent != nullptr
                && !entry.mComponent->isRemoved())
                entry.mComponent->checkLock(*this, entry.mPos);
        }
    }
    mHasBeenEvaluated = true;
}

void CircuitSystem::updateBlocks(int32_t chunkX, int32_t chunkZ, const CircuitBlockUpdate &update) {
    if (!mHasBeenEvaluated)
        return;
    auto &lists = mSceneGraph.getChunkLists();
    const auto found = lists.find(CircuitSceneGraph::chunkKey(chunkX, chunkZ));
    if (found == lists.end())
        return;

    // the update may add or remove components: walk a copy
    const std::vector<CircuitSceneGraph::ChunkEntry> entries = found->second.mEntries;
    std::vector<CircuitSceneGraph::ChunkEntry> secondary;
    const auto notify = [&](BaseCircuitComponent *component, const Vector3i &position) {
        const int strength = component->getStrength();
        const int previous = component->getCachedStrength();
        component->cacheStrength(strength);
        if ((uint16_t) strength == 0xFFFF)
            return;
        if (!(component->isFirstTime() && component->ignoresFirstUpdate()))
            update(position, strength, previous, component->isFirstTime());
        component->clearFirstTime();
    };

    for (const CircuitSceneGraph::ChunkEntry &entry: entries) {
        BaseCircuitComponent *component = mSceneGraph.getComponent(entry.mPos);
        if (component != entry.mComponent || component->isRemoved() || !component->needsBlockUpdate())
            continue;
        component->setNeedsUpdate(false);
        // what a consumer powered from behind does waits for the others
        if (component->isSecondaryPowered()) {
            secondary.push_back(entry);
            continue;
        }
        notify(component, entry.mPos);
    }
    for (const CircuitSceneGraph::ChunkEntry &entry: secondary) {
        BaseCircuitComponent *component = mSceneGraph.getComponent(entry.mPos);
        if (component != entry.mComponent || component->isRemoved())
            continue;
        notify(component, entry.mPos);
    }
}

void CircuitSystem::updateDependencies(CircuitBlockSource *region) {
    mSceneGraph.update(region);
    mHasBeenEvaluated = false;
}

void CircuitSystem::removeChunk(int32_t chunkX, int32_t chunkZ) {
    mSceneGraph.removeChunk(chunkX, chunkZ);
    mUnloadedChunks.insert(CircuitSceneGraph::chunkKey(chunkX, chunkZ));
    // forget the unloaded chunks no chunk with components is near any more
    auto &lists = mSceneGraph.getChunkLists();
    for (auto it = mUnloadedChunks.begin(); it != mUnloadedChunks.end();) {
        const auto x = (int32_t) (*it >> 32);
        const auto z = (int32_t) (uint32_t) *it;
        bool near = false;
        for (int dx = -WAIT_RADIUS_CHUNKS; dx <= WAIT_RADIUS_CHUNKS && !near; ++dx) {
            for (int dz = -WAIT_RADIUS_CHUNKS; dz <= WAIT_RADIUS_CHUNKS && !near; ++dz)
                near = (dx != 0 || dz != 0) && lists.count(CircuitSceneGraph::chunkKey(x + dx, z + dz)) != 0;
        }
        it = near ? std::next(it) : mUnloadedChunks.erase(it);
    }
}

void CircuitSystem::onChunkLoaded(int32_t chunkX, int32_t chunkZ) {
    mUnloadedChunks.erase(CircuitSceneGraph::chunkKey(chunkX, chunkZ));
}

bool CircuitSystem::_isNearUnloaded(int64_t chunk) const {
    if (mUnloadedChunks.empty())
        return false;
    const auto x = (int32_t) (chunk >> 32);
    const auto z = (int32_t) (uint32_t) chunk;
    for (int dx = -WAIT_RADIUS_CHUNKS; dx <= WAIT_RADIUS_CHUNKS; ++dx) {
        for (int dz = -WAIT_RADIUS_CHUNKS; dz <= WAIT_RADIUS_CHUNKS; ++dz) {
            if (mUnloadedChunks.count(CircuitSceneGraph::chunkKey(x + dx, z + dz)) != 0)
                return true;
        }
    }
    return false;
}
