#include "World/Level/Block/Circuit/CircuitSceneGraph.h"

#include <algorithm>

namespace {
    bool samePosition(const Vector3i &a, const Vector3i &b) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }

    Vector3i shifted(const Vector3i &position, int dx, int dy, int dz) {
        return Vector3i(position.x + dx, position.y + dy, position.z + dz);
    }

    bool isSourceType(const BaseCircuitComponent *component) {
        const CircuitComponentType base = component->getBaseType();
        return base == CircuitComponentType::Producer || base == CircuitComponentType::Capacitor;
    }

    bool isLive(const BaseCircuitComponent *component) {
        return component != nullptr && !component->isRemoved();
    }

    // a search entry pointing at one component
    CircuitTrackingInfo::Entry entryOf(BaseCircuitComponent *component, const Vector3i &position, uint8_t face) {
        CircuitTrackingInfo::Entry entry;
        entry.mComponent = component;
        entry.mPos = position;
        entry.mDirection = face;
        entry.mType = component != nullptr ? component->getBaseType() : CircuitComponentType::Undefined;
        return entry;
    }

    // the search lists every component it reached: undone when the source searches again
    void remember(std::vector<CircuitSourceItem> &visited, BaseCircuitComponent *component, const Vector3i &position) {
        CircuitSourceItem item;
        item.mComponent = component;
        item.mPos = position;
        visited.push_back(item);
    }
}

int64_t CircuitSceneGraph::key(const Vector3i &position) {
    const int64_t x = (int64_t) (position.x & 0x3ffffff);
    const int64_t y = (int64_t) (position.y & 0xfff);
    const int64_t z = (int64_t) (position.z & 0x3ffffff);
    return (x << 38) | (y << 26) | z;
}

int64_t CircuitSceneGraph::chunkKey(int32_t chunkX, int32_t chunkZ) {
    return ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
}

BaseCircuitComponent *CircuitSceneGraph::getComponent(const Vector3i &position) const {
    const auto found = mAllComponents.find(key(position));
    return found == mAllComponents.end() ? nullptr : found->second.get();
}

BaseCircuitComponent *CircuitSceneGraph::getPendingComponent(const Vector3i &position) const {
    for (const Pending &pending: mPendingAdds) {
        if (samePosition(pending.mPos, position))
            return pending.mComponent.get();
    }
    return nullptr;
}

BaseCircuitComponent *CircuitSceneGraph::add(const Vector3i &position, std::unique_ptr<BaseCircuitComponent> component) {
    component->setPosition(position);
    for (Pending &pending: mPendingAdds) {
        if (!samePosition(pending.mPos, position))
            continue;
        const bool waitingPowered = pending.mComponent->getBaseType() == CircuitComponentType::PoweredBlock;
        const bool newPowered = component->getBaseType() == CircuitComponentType::PoweredBlock;
        // a placeholder block gives way, except the one that only stops wire stepping down
        if (waitingPowered) {
            if (!pending.mComponent->blocksWireStepDown())
                pending.mComponent = std::move(component);
        } else if (newPowered && component->blocksWireStepDown()) {
            pending.mComponent = std::move(component);
        }
        return pending.mComponent.get();
    }
    mPendingAdds.push_back(Pending{position, std::move(component)});
    return mPendingAdds.back().mComponent.get();
}

void CircuitSceneGraph::remove(const Vector3i &position, BaseCircuitComponent *component) {
    mPendingAdds.erase(std::remove_if(mPendingAdds.begin(), mPendingAdds.end(),
                                      [&](const Pending &pending) { return samePosition(pending.mPos, position); }),
                       mPendingAdds.end());
    if (component == nullptr)
        return;
    component->markRemoved();
    mPendingRemoves.emplace_back(component, position);
}

void CircuitSceneGraph::scheduleRelationshipUpdate(const Vector3i &position) {
    if (mReEvaluateKeys.insert(key(position)).second)
        mComponentsToReEvaluate.push_back(position);
}

void CircuitSceneGraph::invalidatePosition(const Vector3i &position) {
    const BaseCircuitComponent *component = getComponent(position);
    if (isLive(component)) {
        _scheduleSourcesOf(component);
        return;
    }
    for (uint8_t face = 0; face < CircuitFace::NONE; ++face) {
        BaseCircuitComponent *neighbour = getComponent(CircuitFace::relative(position, face));
        if (!isLive(neighbour))
            continue;
        if (isSourceType(neighbour))
            scheduleRelationshipUpdate(neighbour->getPosition());
        else
            _scheduleSourcesOf(neighbour);
    }
}

void CircuitSceneGraph::onChunkLoaded(int32_t chunkX, int32_t chunkZ) {
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            const auto found = mActiveComponentsPerChunk.find(chunkKey(chunkX + dx, chunkZ + dz));
            if (found == mActiveComponentsPerChunk.end())
                continue;
            for (const ChunkEntry &entry: found->second.mEntries)
                _scheduleIfSource(entry.mComponent);
        }
    }
}

void CircuitSceneGraph::_scheduleIfSource(BaseCircuitComponent *component) {
    if (isLive(component) && isSourceType(component))
        scheduleRelationshipUpdate(component->getPosition());
}

void CircuitSceneGraph::_scheduleSourcesOf(const BaseCircuitComponent *component) {
    for (const CircuitSourceItem &item: component->getSources()) {
        BaseCircuitComponent *source = getComponent(item.mPos);
        if (isLive(source) && isSourceType(source))
            scheduleRelationshipUpdate(item.mPos);
    }
}

void CircuitSceneGraph::_insert(const Vector3i &position, std::unique_ptr<BaseCircuitComponent> component) {
    component->setPosition(position);
    mAllComponents[key(position)] = std::move(component);
}

void CircuitSceneGraph::_processRemoves() {
    std::vector<std::pair<BaseCircuitComponent *, Vector3i>> removes = std::move(mPendingRemoves);
    mPendingRemoves.clear();
    for (const auto &[component, position]: removes) {
        const auto found = mAllComponents.find(key(position));
        if (found == mAllComponents.end() || found->second.get() != component)
            continue;
        std::unique_ptr<BaseCircuitComponent> owned = std::move(found->second);
        mAllComponents.erase(found);
        if (owned->getBaseType() != CircuitComponentType::PoweredBlock)
            mPowerAssociationMap.erase(key(position));

        for (auto &chunk: mActiveComponentsPerChunk) {
            std::vector<ChunkEntry> &entries = chunk.second.mEntries;
            entries.erase(std::remove_if(entries.begin(), entries.end(),
                                         [&](const ChunkEntry &entry) { return samePosition(entry.mPos, position); }),
                          entries.end());
        }

        for (const CircuitSourceItem &item: owned->getSources()) {
            if (item.mComponent != nullptr && isSourceType(item.mComponent)) {
                scheduleRelationshipUpdate(item.mPos);
            }
            if (item.mComponent != nullptr)
                item.mComponent->forgetDestination(owned.get());
        }

        // whoever took power from it forgets it, and searches again when it is a source
        const std::map<BaseCircuitComponent *, int> destinations = owned->getDestinations();
        for (const auto &destination: destinations) {
            BaseCircuitComponent *target = destination.first;
            target->removeSource(position, owned.get());
            target->removeFromAnySourceList(owned.get());
            if (isSourceType(target))
                scheduleRelationshipUpdate(target->getPosition());
        }

        for (uint8_t face = 0; face < CircuitFace::NONE; ++face) {
            BaseCircuitComponent *neighbour = getComponent(CircuitFace::relative(position, face));
            if (!isLive(neighbour))
                continue;
            if (isSourceType(neighbour))
                scheduleRelationshipUpdate(neighbour->getPosition());
            _scheduleSourcesOf(neighbour);
        }

        if (mReEvaluateKeys.erase(key(position)) != 0) {
            mComponentsToReEvaluate.erase(
                    std::remove_if(mComponentsToReEvaluate.begin(), mComponentsToReEvaluate.end(),
                                   [&](const Vector3i &scheduled) { return samePosition(scheduled, position); }),
                    mComponentsToReEvaluate.end());
        }
    }
}

void CircuitSceneGraph::_scheduleAround(const Vector3i &position, BaseCircuitComponent *component) {
    (void) component;
    for (int dy = -1; dy <= 1; ++dy) {
        for (uint8_t face = 0; face < CircuitFace::NONE; ++face) {
            const Vector3i neighbourPosition = shifted(CircuitFace::relative(position, face), 0, dy, 0);
            BaseCircuitComponent *neighbour = getComponent(neighbourPosition);
            if (!isLive(neighbour))
                continue;
            if (isSourceType(neighbour)) {
                scheduleRelationshipUpdate(neighbourPosition);
                continue;
            }
            _scheduleSourcesOf(neighbour);
        }
    }
}

void CircuitSceneGraph::_processAdds() {
    std::vector<Pending> adds = std::move(mPendingAdds);
    mPendingAdds.clear();
    for (Pending &pending: adds) {
        const Vector3i position = pending.mPos;
        BaseCircuitComponent *existing = getComponent(position);
        const bool replacing = existing != nullptr && existing->getBaseType() == CircuitComponentType::PoweredBlock
                               && pending.mComponent->getBaseType() != CircuitComponentType::PoweredBlock;
        if (existing != nullptr && !replacing)
            continue;
        if (replacing) {
            for (auto &entry: mAllComponents) {
                if (entry.second.get() != existing && entry.second->hasSource(*existing))
                    entry.second->removeSource(position, existing);
            }
            mAllComponents.erase(key(position));
        }

        BaseCircuitComponent *component = pending.mComponent.get();
        _insert(position, std::move(pending.mComponent));
        const CircuitComponentType base = component->getBaseType();
        if (base != CircuitComponentType::PoweredBlock) {
            ChunkList &list = mActiveComponentsPerChunk[chunkKey(position.x >> 4, position.z >> 4)];
            uint8_t flags = component->getCircuitFlags();
            flags |= base == CircuitComponentType::Producer || base == CircuitComponentType::Capacitor
                             ? CircuitFlags::PRODUCER_PASS
                             : CircuitFlags::CONSUMER_PASS;
            list.mEntries.push_back(ChunkEntry{component, position, flags});

            if (base == CircuitComponentType::Producer) {
                scheduleRelationshipUpdate(position);
            } else if (base == CircuitComponentType::Capacitor) {
                scheduleRelationshipUpdate(position);
                // a new capacitor wakes the capacitors on its diagonals: their side inputs may change
                for (int dx = -1; dx <= 1; dx += 2) {
                    for (int dz = -1; dz <= 1; dz += 2) {
                        BaseCircuitComponent *diagonal = getComponent(shifted(position, dx, 0, dz));
                        if (isLive(diagonal) && diagonal->getBaseType() == CircuitComponentType::Capacitor)
                            scheduleRelationshipUpdate(diagonal->getPosition());
                    }
                }
            }
        }
        _scheduleAround(position, component);
    }
}

BaseCircuitComponent *CircuitSceneGraph::_componentOfType(const Vector3i &position, CircuitComponentType type) const {
    BaseCircuitComponent *component = getComponent(position);
    if (!isLive(component))
        return nullptr;
    if (component->getBaseType() == type || component->getInstanceType() == type)
        return component;
    return nullptr;
}

void CircuitSceneGraph::_step(std::vector<CircuitSourceItem> &visited, BaseCircuitComponent *component,
                              const CircuitTrackingInfo &from, const Vector3i &position, uint8_t face,
                              std::deque<CircuitTrackingInfo> &queue) {
    if (component == nullptr)
        return;
    CircuitTrackingInfo info = from;
    info.mCurrent = entryOf(component, position, face);
    int dampening = from.mDampening;
    bool directlyPowered = from.mDirectlyPowered;
    remember(visited, component, position);
    if (!from.mNearest.mComponent->allowConnection(*this, info, directlyPowered))
        return;
    if (!component->addSource(*this, info, dampening, directlyPowered))
        return;
    info.m2ndNearest = from.mNearest;
    info.mNearest = info.mCurrent;
    info.mDampening = dampening;
    info.mDirectlyPowered = directlyPowered;
    queue.push_back(info);
}

// a conductive block next to the search becomes a powered block the moment the search reaches it
void CircuitSceneGraph::_addPoweredBlocksAround(const Vector3i &position, CircuitBlockSource *region) {
    if (region == nullptr)
        return;
    for (uint8_t face = 0; face < CircuitFace::NONE; ++face) {
        const Vector3i neighbour = CircuitFace::relative(position, face);
        BaseCircuitComponent *existing = getComponent(neighbour);
        if (existing != nullptr && !existing->isRemoved())
            continue;
        if (!region->isLoaded(neighbour) || region->isAir(neighbour))
            continue;
        const RedstoneConductivity conductivity = region->getConductivity(neighbour);
        if (!conductivity.mConductor && conductivity.mAllowsWireToStepDown)
            continue;
        auto block = std::make_unique<PoweredBlockComponent>();
        if (!conductivity.mAllowsWireToStepDown) {
            block->setBlocksWireStepDown(true);
            block->setAllowAsPowerSource(false);
        }
        _insert(neighbour, std::move(block));
    }
}

void CircuitSceneGraph::_searchFrom(BaseCircuitComponent &root, const Vector3i &position, CircuitBlockSource *region) {
    std::vector<CircuitSourceItem> &visited = mPowerAssociationMap[key(position)];
    std::deque<CircuitTrackingInfo> queue;
    CircuitTrackingInfo start;
    start.mCurrent = entryOf(&root, position, root.getDirection());
    start.mPower = start.mCurrent;
    start.mNearest = start.mCurrent;
    start.m2ndNearest = start.mCurrent;
    start.mDampening = 0;
    start.mDirectlyPowered = true;
    queue.push_back(start);

    while (!queue.empty()) {
        const CircuitTrackingInfo current = queue.front();
        queue.pop_front();
        if (current.mDampening > 15)
            continue;

        BaseCircuitComponent *nearest = current.mNearest.mComponent;
        const Vector3i at = current.mNearest.mPos;
        if (nearest->getBaseType() != CircuitComponentType::PoweredBlock)
            _addPoweredBlocksAround(at, region);

        for (uint8_t face = 0; face < CircuitFace::NONE; ++face) {
            const Vector3i next = CircuitFace::relative(at, face);
            BaseCircuitComponent *component = getComponent(next);
            if (!isLive(component) || samePosition(next, current.m2ndNearest.mPos))
                continue;
            _step(visited, component, current, next, face, queue);
        }

        const CircuitComponentType type = nearest->getBaseType();
        if (type == CircuitComponentType::Producer && nearest->getDirection() == CircuitFace::DOWN) {
            // a producer on a block reaches the wire around the block it stands on
            const Vector3i below = shifted(at, 0, -1, 0);
            auto *over = static_cast<PoweredBlockComponent *>(
                    _componentOfType(shifted(at, 0, 1, 0), CircuitComponentType::PoweredBlock));
            auto *under = static_cast<PoweredBlockComponent *>(_componentOfType(below, CircuitComponentType::PoweredBlock));
            if ((over != nullptr || under != nullptr) && (under == nullptr || under->allowsAsPowerSource())
                && (over == nullptr || over->allowsAsPowerSource()) && (under == nullptr || !under->blocksWireStepDown())
                && (over == nullptr || over->blocksWireStepDown() || over->isPromotedToProducer())) {
                for (uint8_t face = CircuitFace::NORTH; face <= CircuitFace::EAST; ++face) {
                    const Vector3i wire = CircuitFace::relative(below, face);
                    _step(visited, _componentOfType(wire, CircuitComponentType::Transporter), current, wire, face, queue);
                }
            }
        } else if (type == CircuitComponentType::Transporter) {
            BaseCircuitComponent *above = getComponent(shifted(at, 0, 1, 0));
            if (!isLive(above) || above->getBaseType() != CircuitComponentType::PoweredBlock
                || above->blocksWireStepDown()) {
                for (uint8_t face = CircuitFace::NORTH; face <= CircuitFace::EAST; ++face) {
                    const Vector3i wire = shifted(CircuitFace::relative(at, face), 0, 1, 0);
                    _step(visited, _componentOfType(wire, CircuitComponentType::Transporter), current, wire, face, queue);
                }
            }
            BaseCircuitComponent *below = getComponent(shifted(at, 0, -1, 0));
            const bool support = isLive(below) && below->getBaseType() == CircuitComponentType::PoweredBlock
                                 && below->blocksWireStepDown();
            if (!support) {
                for (uint8_t face = CircuitFace::NORTH; face <= CircuitFace::EAST; ++face) {
                    const Vector3i wire = shifted(CircuitFace::relative(at, face), 0, -1, 0);
                    _step(visited, _componentOfType(wire, CircuitComponentType::Transporter), current, wire, face, queue);
                }
            }
        } else if (nearest->getInstanceType() == CircuitComponentType::RailTransporter) {
            // rails reach the rails one step up or down their slopes
            for (int dy = 1; dy >= -1; dy -= 2) {
                for (uint8_t face = CircuitFace::NORTH; face <= CircuitFace::EAST; ++face) {
                    const Vector3i rail = shifted(CircuitFace::relative(at, face), 0, dy, 0);
                    _step(visited, _componentOfType(rail, CircuitComponentType::RailTransporter), current, rail, face,
                          queue);
                }
            }
        }
    }
}

void CircuitSceneGraph::_rebuildRelationships(CircuitBlockSource *region) {
    const std::vector<Vector3i> scheduled = std::move(mComponentsToReEvaluate);
    mComponentsToReEvaluate.clear();
    mReEvaluateKeys.clear();

    for (const Vector3i &position: scheduled) {
        const auto found = mPowerAssociationMap.find(key(position));
        if (found == mPowerAssociationMap.end())
            continue;
        BaseCircuitComponent *root = getComponent(position);
        for (const CircuitSourceItem &item: found->second) {
            BaseCircuitComponent *reached = getComponent(item.mPos);
            if (reached != nullptr)
                reached->removeSource(position, root);
        }
        found->second.clear();
    }

    for (const Vector3i &position: scheduled) {
        BaseCircuitComponent *root = getComponent(position);
        if (!isLive(root))
            continue;
        _searchFrom(*root, position, region);
    }

    for (auto &entry: mAllComponents)
        entry.second->updateDependencies(*this, entry.second->getPosition());
}

void CircuitSceneGraph::update(CircuitBlockSource *region) {
    _processRemoves();
    _processAdds();
    if (mComponentsToReEvaluate.empty())
        return;
    _rebuildRelationships(region);
}

void CircuitSceneGraph::removeChunk(int32_t chunkX, int32_t chunkZ) {
    for (auto &entry: mAllComponents) {
        const Vector3i &position = entry.second->getPosition();
        if ((position.x >> 4) == chunkX && (position.z >> 4) == chunkZ && !entry.second->isRemoved())
            remove(position, entry.second.get());
    }
}

bool CircuitSceneGraph::isWireConnected(const Vector3i &position, uint8_t face) const {
    const Vector3i side = CircuitFace::relative(position, face);

    bool down = false;
    const BaseCircuitComponent *lower = getComponent(shifted(side, 0, -1, 0));
    if (isLive(lower) && lower->getInstanceType() != CircuitComponentType::Repeater
        && lower->getBaseType() == CircuitComponentType::Transporter
        && lower->getInstanceType() != CircuitComponentType::Pulse) {
        const BaseCircuitComponent *between = getComponent(side);
        down = !isLive(between) || between->blocksWireStepDown();
    }

    const BaseCircuitComponent *upper = getComponent(shifted(side, 0, 1, 0));
    if (isLive(upper) && upper->getInstanceType() != CircuitComponentType::Repeater
        && upper->getBaseType() == CircuitComponentType::Transporter
        && upper->getInstanceType() != CircuitComponentType::Pulse) {
        const BaseCircuitComponent *over = getComponent(shifted(position, 0, 1, 0));
        if (!isLive(over) || over->blocksWireStepDown() || down)
            return true;
    } else if (down) {
        return true;
    }

    const BaseCircuitComponent *beside = getComponent(side);
    if (!isLive(beside))
        return false;
    const CircuitComponentType instance = beside->getInstanceType();
    if (instance == CircuitComponentType::Repeater) {
        const uint8_t out = beside->getPoweroutDirection();
        return face == out || face == CircuitFace::opposite(out);
    }
    if (instance == CircuitComponentType::Pulse)
        return face == CircuitFace::opposite(beside->getPoweroutDirection());
    const CircuitComponentType base = beside->getBaseType();
    return base == CircuitComponentType::Transporter || base == CircuitComponentType::Capacitor
           || base == CircuitComponentType::Producer;
}

bool CircuitSceneGraph::isWirePointingAt(const Vector3i &position, const Vector3i &delta) const {
    const bool north = isWireConnected(position, CircuitFace::NORTH);
    const bool south = isWireConnected(position, CircuitFace::SOUTH);
    const bool east = isWireConnected(position, CircuitFace::EAST);
    const bool west = isWireConnected(position, CircuitFace::WEST);
    const int count = (north ? 1 : 0) + (south ? 1 : 0) + (east ? 1 : 0) + (west ? 1 : 0);
    if (count == 0)
        return true;
    if (count != 1)
        return false;
    return ((delta.x != 0 && east) != (delta.x != 0 && west)) || ((delta.z != 0 && north) != (delta.z != 0 && south));
}
