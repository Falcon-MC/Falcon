#pragma once

#include "World/Level/Block/Actor/BlockActor.h"
#include "Core/Math/Vector3f.h"

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <vector>

class ServerPlayer;

// the monster spawner: counts down while a player is in range, then spawns its mob around the block; the client
// spins the mob and draws the flames from the data it is sent
class MobSpawnerBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "MobSpawner";

    // the block event that resets the client's spawner animation
    static constexpr int32_t SPAWN_EVENT = 1;

    // the level event of the spawn puff, its data the mob's width and height rounded up
    static constexpr int32_t MOB_SPAWN_LEVEL_EVENT = 2004;

    struct SpawnPotential {
        int32_t mWeight = 0;
        std::string mIdentifier;
        // the entry as read, written back untouched
        Tag mData;
    };

    MobSpawnerBlockActor();

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    Tag getSpawnCompound() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    bool tick(ServerNetworkHandler &owner) override;

    // what the spawn egg sets; a chosen spawn entry still wins, as in the game
    void setEntityIdentifier(const std::string &identifier);

    // the mob of the next spawn: the chosen spawn entry when there is one
    const std::string &getSpawnIdentifier() const;

    const std::string &getEntityIdentifier() const noexcept {
        return mEntityIdentifier;
    }

    bool hasEntity() const noexcept {
        return mHasEntity;
    }

    int32_t getDelay() const noexcept {
        return mDelay;
    }

    void setDelay(int32_t delay) {
        mDelay = delay;
    }

    int32_t getMinSpawnDelay() const noexcept {
        return mMinSpawnDelay;
    }

    int32_t getMaxSpawnDelay() const noexcept {
        return mMaxSpawnDelay;
    }

    int32_t getSpawnCount() const noexcept {
        return mSpawnCount;
    }

    int32_t getMaxNearbyEntities() const noexcept {
        return mMaxNearbyEntities;
    }

    int32_t getRequiredPlayerRange() const noexcept {
        return mRequiredPlayerRange;
    }

    int32_t getSpawnRange() const noexcept {
        return mSpawnRange;
    }

    const std::vector<SpawnPotential> &getSpawnPotentials() const noexcept {
        return mSpawnPotentials;
    }

    void seedRandom(uint32_t seed) {
        mRandom.seed(seed);
    }

    // a player standing close enough: the spawner only counts down and spawns while one is
    bool isNearPlayer(ServerNetworkHandler &owner, Level &level) const;

    // the mobs of the spawner's kind whose box meets the box the cap is counted in
    int32_t countNearby(ServerNetworkHandler &owner, Level &level, const std::string &identifier) const;

private:
    void _writeFields(Tag &data) const;

    // a new delay, and the next spawn entry drawn from the potentials
    void _resetDelay(ServerNetworkHandler &owner, Level &level);

    bool _trySpawn(ServerNetworkHandler &owner, Level &level, const std::string &identifier, bool &capped);

    int32_t _nextInt(int32_t bound);

    float _nextFloat();

    int32_t mDelay = 20;
    int32_t mMinSpawnDelay = 200;
    int32_t mMaxSpawnDelay = 800;
    int32_t mSpawnCount = 4;
    int32_t mMaxNearbyEntities = 6;
    int32_t mRequiredPlayerRange = 16;
    int32_t mSpawnRange = 4;
    float mDisplayWidth = 0.8f;
    float mDisplayHeight = 1.8f;
    float mDisplayScale = 1.0f;
    std::string mEntityIdentifier;
    bool mHasEntity = false;
    std::vector<SpawnPotential> mSpawnPotentials;
    std::optional<SpawnPotential> mSpawnData;
    std::mt19937 mRandom;
};
