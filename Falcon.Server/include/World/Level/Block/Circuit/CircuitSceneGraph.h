#pragma once

#include "World/Level/Block/Circuit/CircuitComponent.h"
#include "World/Level/Block/Circuit/RedstoneConductivity.h"

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// what the circuit asks the world about the blocks it has no component for
class CircuitBlockSource {
public:
    virtual ~CircuitBlockSource() = default;

    virtual bool isLoaded(const Vector3i &position) const = 0;

    virtual bool isAir(const Vector3i &position) const = 0;

    virtual RedstoneConductivity getConductivity(const Vector3i &position) const = 0;
};

class CircuitSceneGraph {
public:
    struct ChunkEntry {
        BaseCircuitComponent *mComponent = nullptr;
        Vector3i mPos;
        uint8_t mFlags = 0;
    };

    struct ChunkList {
        std::vector<ChunkEntry> mEntries;
        bool mShouldEvaluate = true;
    };

    static int64_t key(const Vector3i &position);

    static int64_t chunkKey(int32_t chunkX, int32_t chunkZ);

    BaseCircuitComponent *getComponent(const Vector3i &position) const;

    BaseCircuitComponent *getPendingComponent(const Vector3i &position) const;

    // takes the component into the pending adds; returns the one waiting at that position
    BaseCircuitComponent *add(const Vector3i &position, std::unique_ptr<BaseCircuitComponent> component);

    void remove(const Vector3i &position, BaseCircuitComponent *component);

    void update(CircuitBlockSource *region);

    // the source at `position` rebuilds its relationships on the next update
    void scheduleRelationshipUpdate(const Vector3i &position);

    // the block at `position` changed: the sources it or its neighbours depend on search again
    void invalidatePosition(const Vector3i &position);

    // the sources of a chunk and its neighbours search again: a search may have stopped at its border
    void onChunkLoaded(int32_t chunkX, int32_t chunkZ);

    // the wire at `position` connects toward `face`, the way the game draws it
    bool isWireConnected(const Vector3i &position, uint8_t face) const;

    // the wire at `position` points at the block `delta` away: a dot points everywhere
    bool isWirePointingAt(const Vector3i &position, const Vector3i &delta) const;

    // the comparators, which read the block behind them every tick
    const std::unordered_map<int64_t, BaseCircuitComponent *> &getComparators() const {
        return mComparators;
    }

    std::map<int64_t, ChunkList> &getChunkLists() {
        return mActiveComponentsPerChunk;
    }

    // components and pending work of one chunk column
    void removeChunk(int32_t chunkX, int32_t chunkZ);

    size_t getComponentCount() const {
        return mAllComponents.size();
    }

private:
    struct Pending {
        Vector3i mPos;
        std::unique_ptr<BaseCircuitComponent> mComponent;
    };

    void _processRemoves();

    void _processAdds();

    void _scheduleAround(const Vector3i &position, BaseCircuitComponent *component);

    void _scheduleSourcesOf(const BaseCircuitComponent *component);

    void _scheduleIfSource(BaseCircuitComponent *component);

    void _rebuildRelationships(CircuitBlockSource *region);

    void _searchFrom(BaseCircuitComponent &root, const Vector3i &position, CircuitBlockSource *region);

    void _addPoweredBlocksAround(const Vector3i &position, CircuitBlockSource *region);

    void _step(std::vector<CircuitSourceItem> &visited, BaseCircuitComponent *component, const CircuitTrackingInfo &from,
               const Vector3i &position, uint8_t face, std::deque<CircuitTrackingInfo> &queue);

    BaseCircuitComponent *_componentOfType(const Vector3i &position, CircuitComponentType type) const;

    void _insert(const Vector3i &position, std::unique_ptr<BaseCircuitComponent> component);

    // drops the component at `position` from the maps; the caller holds what it returns
    std::unique_ptr<BaseCircuitComponent> _erase(const Vector3i &position);

    std::unordered_map<int64_t, std::unique_ptr<BaseCircuitComponent>> mAllComponents;
    // the positions of every component of each chunk, powered blocks included, for a chunk unload
    std::unordered_map<int64_t, std::unordered_set<int64_t>> mComponentsPerChunk;
    std::unordered_map<int64_t, BaseCircuitComponent *> mComparators;
    std::map<int64_t, ChunkList> mActiveComponentsPerChunk;
    // every component a source reached on its last search
    std::unordered_map<int64_t, std::vector<CircuitSourceItem>> mPowerAssociationMap;
    std::vector<Pending> mPendingAdds;
    std::vector<std::pair<BaseCircuitComponent *, Vector3i>> mPendingRemoves;
    std::vector<Vector3i> mComponentsToReEvaluate;
    std::unordered_set<int64_t> mReEvaluateKeys;
};
