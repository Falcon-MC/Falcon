#pragma once

#include "World/Level/Block/BlockState.h"
#include "Core/Math/Vector3i.h"
#include "Core/NBT/Tag.h"
#include "World/Inventory/Container.h"
#include "World/Actor/DynamicPropertyValue.h"
#include "World/Level/Block/Actor/BlockEntityStorage.h"

#include <optional>
#include <string>
#include <unordered_map>

class GameEventListener;
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

    virtual Container *getContainer() {
        return nullptr;
    }

    // what the chunk's game events reach, as the game keeps a listener registry per chunk
    virtual GameEventListener *getGameEventListener() {
        return nullptr;
    }

    virtual bool tick(ServerNetworkHandler &owner) {
        (void) owner;
        return true;
    }

    const BlockState &getState() const noexcept {
        return mState;
    }

    void setState(const BlockState &state) {
        mState = state;
    }

    const Vector3i &getPosition() const noexcept {
        return mPosition;
    }

    void setPosition(const Vector3i &position) {
        mPosition = position;
    }

    Level *getLevel() const noexcept {
        return mLevel;
    }

    void setLevel(Level *level) {
        mLevel = level;
    }

    Tag saveWithPosition() const;

    // rolled by the game on first access; seed 0: unseeded roll
    void setLootTable(const std::string &table, int32_t seed);

    void loadLootTable(const Tag &data);

    void unpackLoot(ServerNetworkHandler &owner);

    void loadDynamicProperties(const Tag &data);

    std::unordered_map<std::string, DynamicPropertyValue> &getDynamicProperties() {
        return mDynamicProperties;
    }

    // any block actor can carry one (sub_143300CF0 saves it with the common fields), data-driven blocks ask for it
    BlockEntityStorage &enableEntityStorage() {
        if (!mEntityStorage.has_value())
            mEntityStorage.emplace();
        return *mEntityStorage;
    }

    BlockEntityStorage *getEntityStorage() {
        return mEntityStorage.has_value() ? &*mEntityStorage : nullptr;
    }

    void loadEntityStorage(const Tag &data);

protected:
    Vector3i mPosition;
    Level *mLevel = nullptr;

private:
    BlockState mState;
    std::string mLootTable;
    int32_t mLootSeed = 0;
    std::unordered_map<std::string, DynamicPropertyValue> mDynamicProperties;
    std::optional<BlockEntityStorage> mEntityStorage;
};
