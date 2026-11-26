#pragma once

#include "Core/Math/Vector3i.h"
#include "World/Level/Block/BlockState.h"

#include <cstdint>

class GenericLevelEventParticle;
class Level;
class ServerNetworkHandler;

// the blocks a sculk spreader reads and changes: a level, or the chunks a generator decorates
class SculkWorld {
public:
    virtual ~SculkWorld() = default;

    // block states are interned: the reference stays valid after a block is set
    virtual const BlockState &getBlock(const Vector3i &position) = 0;

    virtual void setBlock(const Vector3i &position, const BlockState &state) = 0;

    // water on either layer
    virtual bool isWaterSource(const Vector3i &position) = 0;

    // a sensor or a shrieker grown over sculk
    virtual void placeGrowth(const Vector3i &position, const BlockState &state) {
        setBlock(position, state);
    }

    // a cursor outside the ticking area keeps its charge; world generation moves every cursor
    virtual bool isTicking(const Vector3i &) {
        return true;
    }

    virtual void playSound(const char *, const Vector3i &) {}

    virtual void addParticle(const GenericLevelEventParticle &) {}
};

// a running level: block updates, sounds for the players and the charge particles
class LevelSculkWorld : public SculkWorld {
public:
    LevelSculkWorld(ServerNetworkHandler *owner, Level &level) : mOwner(owner), mLevel(level) {}

    const BlockState &getBlock(const Vector3i &position) override;

    void setBlock(const Vector3i &position, const BlockState &state) override;

    bool isWaterSource(const Vector3i &position) override;

    void placeGrowth(const Vector3i &position, const BlockState &state) override;

    bool isTicking(const Vector3i &position) override;

    void playSound(const char *sound, const Vector3i &position) override;

    void addParticle(const GenericLevelEventParticle &particle) override;

private:
    ServerNetworkHandler *mOwner;
    Level &mLevel;
};
