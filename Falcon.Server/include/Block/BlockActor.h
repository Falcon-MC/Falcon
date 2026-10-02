#pragma once

#include "Block/BlockState.h"
#include "Core/Math/Vector3i.h"
#include "Core/NBT/Tag.h"
#include "Inventory/Container.h"

#include <string>

class Level;
class PacketCodecContext;
class ServerNetworkHandler;

class BlockActor {
public:
    BlockActor() = default;

    explicit BlockActor(const BlockState &state) : mState(state) {}

    virtual ~BlockActor() = default;

    virtual const char *getBlockActorId() const = 0;

    virtual Tag saveNbt() const = 0;

    virtual Tag getSpawnCompound() const;

    virtual void loadNbt(const Tag &data, const PacketCodecContext &context) = 0;

    virtual Container *getContainer() { return nullptr; }

    virtual bool tick(ServerNetworkHandler &owner) {
        (void) owner;
        return true;
    }

    const BlockState &getState() const noexcept { return mState; }

    void setState(const BlockState &state) { mState = state; }

    const Vector3i &getPosition() const noexcept { return mPosition; }

    void setPosition(const Vector3i &position) { mPosition = position; }

    Level *getLevel() const noexcept { return mLevel; }

    void setLevel(Level *level) { mLevel = level; }

    Tag saveWithPosition() const;

protected:
    Vector3i mPosition;
    Level *mLevel = nullptr;

private:
    BlockState mState;
};
