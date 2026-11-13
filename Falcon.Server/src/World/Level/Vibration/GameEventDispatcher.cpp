#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"
#include "World/Level/Vibration/GameEventListener.h"

#include <cmath>
#include <vector>

namespace {
    constexpr float NOTIFY_RADIUS = 16.0f;
    constexpr uint64_t MAX_SQUARED_DISTANCE = 256;
}

void Level::postGameEvent(const GameEvent &event, const Vector3f &position, Actor *source, const BlockState *block) {
    GameEventContext context;
    context.mPosition = position;
    context.mSource = source;
    context.mBlock = block;

    const int32_t minChunkX = (int32_t) std::floor(position.x - NOTIFY_RADIUS) >> 4;
    const int32_t maxChunkX = (int32_t) std::floor(position.x + NOTIFY_RADIUS) >> 4;
    const int32_t minChunkZ = (int32_t) std::floor(position.z - NOTIFY_RADIUS) >> 4;
    const int32_t maxChunkZ = (int32_t) std::floor(position.z + NOTIFY_RADIUS) >> 4;

    // gathered first: a listener reacting can add or remove block actors
    std::vector<GameEventListener *> listeners;
    for (int32_t chunkX = minChunkX; chunkX <= maxChunkX; ++chunkX) {
        for (int32_t chunkZ = minChunkZ; chunkZ <= maxChunkZ; ++chunkZ) {
            mBlockActors.forEachInChunk(chunkX, chunkZ, [&listeners](BlockActor &blockActor) {
                GameEventListener *listener = blockActor.getGameEventListener();
                if (listener != nullptr)
                    listeners.push_back(listener);
            });
        }
    }

    for (GameEventListener *listener: listeners) {
        Vector3f at;
        if (!listener->getPosition(*this, at))
            continue;

        const float dx = position.x - at.x;
        const float dy = position.y - at.y;
        const float dz = position.z - at.z;
        // the squared distance truncated to a whole number before both range checks
        const uint64_t squared = (uint64_t) (dz * dz + (dy * dy + dx * dx));
        const uint64_t range = listener->getRange();
        if (squared > MAX_SQUARED_DISTANCE || range * range < squared)
            continue;
        listener->handleGameEvent(*this, event, context);
    }
}
