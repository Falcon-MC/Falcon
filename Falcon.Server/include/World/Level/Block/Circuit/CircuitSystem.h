#pragma once

#include "World/Level/Block/Circuit/CircuitSceneGraph.h"

#include <functional>
#include <memory>

// strength is the component's, previous the one the last update saw
using CircuitBlockUpdate = std::function<void(const Vector3i &position, int strength, int previous, bool firstTime)>;

class CircuitSystem {
public:
    // the dimension evaluates its circuit every second game tick
    static constexpr int TICK_RATE = 2;

    template <typename T>
    T *create(const Vector3i &position, uint8_t direction) {
        auto component = std::make_unique<T>();
        const CircuitComponentType type = component->getInstanceType();
        // the one already waiting at the position wins: the new one is dropped
        BaseCircuitComponent *placed = _create(position, direction, std::move(component));
        return placed != nullptr && placed->getInstanceType() == type ? static_cast<T *>(placed) : nullptr;
    }

    void removeComponents(const Vector3i &position);

    BaseCircuitComponent *getComponent(const Vector3i &position) const;

    // the component at `position`, placed or still waiting for the graph
    BaseCircuitComponent *findComponent(const Vector3i &position) const;

    // -1 without a component
    int getStrength(const Vector3i &position) const;

    void setStrength(const Vector3i &position, int strength);

    bool isLocked() const {
        return mLockGraph;
    }

    // block changes made while it is set don't touch the graph
    void setLocked(bool locked) {
        mLockGraph = locked;
    }

    // the dimension's redstone tick: true when the circuit was evaluated
    bool tickRedstone(CircuitBlockSource *region);

    void evaluate(CircuitBlockSource *region);

    // the chunk's consumers and changed components hear their new strength
    void updateBlocks(int32_t chunkX, int32_t chunkZ, const CircuitBlockUpdate &update);

    // the level's redstone tick: components added and removed join the graph
    void updateDependencies(CircuitBlockSource *region);

    bool hasBeenEvaluated() const {
        return mHasBeenEvaluated;
    }

    void invalidate() {
        mHasBeenEvaluated = false;
    }

    CircuitSceneGraph &getSceneGraph() {
        return mSceneGraph;
    }

    void removeChunk(int32_t chunkX, int32_t chunkZ);

private:
    BaseCircuitComponent *_create(const Vector3i &position, uint8_t direction,
                                  std::unique_ptr<BaseCircuitComponent> component);

    void _evaluateComponents(bool producers);

    bool mLockGraph = false;
    CircuitSceneGraph mSceneGraph;
    bool mHasBeenEvaluated = true;
    int mTickCounter = TICK_RATE;
};
