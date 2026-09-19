#pragma once

#include "Actor/ServerActor.h"
#include "Block/BlockState.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>

class FallingBlockActor : public ServerActor {
public:
    static const char *IDENTIFIER;

    static const float GRAVITY;
    static const float DRAG;
    static const float BASE_OFFSET;
    static const int32_t ALIVE_TICK_LIMIT = 600;
    static const int32_t ANVIL_BREAK_FALL_DISTANCE = 8;
    static const float ANVIL_DAMAGE_PER_FALL_DISTANCE;
    static const float ANVIL_MAX_DAMAGE;
    static const float ANVIL_LAND_VOLUME;
    static const float ANVIL_LAND_PITCH;

    FallingBlockActor(uint64_t runtimeId, const BlockState &blockState);

    const BlockState &getBlockState() const { return mBlockState; }

    void setBlockState(const BlockState &state) { mBlockState = state; }

    bool getBreakOnLava() const { return mBreakOnLava; }

    void setBreakOnLava(bool value) { mBreakOnLava = value; }

    bool getBreakOnGround() const { return mBreakOnGround; }

    void setBreakOnGround(bool value) { mBreakOnGround = value; }

    void tick(ServerNetworkHandler &owner) override;

    bool isExpired() const override { return mExpired; }

    float getBaseOffset() const override {
        return BASE_OFFSET;
    }

    bool shouldSave() const override { return !mExpired; }

    void fillSpawnMetadata(EntityDataMap &metadata) const override;

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

private:
    bool _hasAliveLimit() const;

    Vector3i _restingPosition() const;

    void _destroy(ServerNetworkHandler &owner, const Vector3i &position, bool dropItem);

    void _land(ServerNetworkHandler &owner, const Vector3i &position);

    void _place(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state);

    void _dropItem(ServerNetworkHandler &owner);

    void _onAnvilLanded(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state);

    void _onDripstoneLanded(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state);

    void _damageEntitiesAt(ServerNetworkHandler &owner, const Vector3i &position, const std::string &deathKey);

    BlockState mBlockState;
    bool mBreakOnLava = false;
    bool mBreakOnGround = false;
    int32_t mAliveTicks = 0;
    bool mExpired = false;
};
