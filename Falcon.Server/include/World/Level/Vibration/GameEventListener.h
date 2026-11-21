#pragma once

#include "Core/Math/Vector3f.h"

#include <cstdint>

class Actor;
class BlockState;
class GameEvent;
class Level;

struct GameEventContext {
    Vector3f mPosition;
    Actor *mSource = nullptr;
    // the block the event is about, when there is one
    const BlockState *mBlock = nullptr;
};

// what the dispatcher reaches (IGameEventListener): it filters on getRange, the listener on everything else
class GameEventListener {
public:
    virtual ~GameEventListener() = default;

    virtual void handleGameEvent(Level &level, const GameEvent &event, const GameEventContext &context) = 0;

    // false when the listener has no position this tick
    virtual bool getPosition(Level &level, Vector3f &position) const = 0;

    virtual uint32_t getRange() const = 0;
};
