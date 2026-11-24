#pragma once

#include "World/Level/Block/Actor/BlockActor.h"
#include "World/Level/Vibration/VibrationListener.h"

#include <cstdint>
#include <optional>

// SculkSensorBlockActor: the block's VibrationListener and the power it gives out
class SculkSensorBlockActor : public BlockActor, public VibrationListenerConfig {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "SculkSensor";
    static constexpr uint32_t RANGE = 8;

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    bool tick(ServerNetworkHandler &owner) override;

    GameEventListener *getGameEventListener() override {
        return &listener();
    }

    bool shouldListen(Level &level, const GameEvent &event, const GameEventContext &context) override;

    void onSignalReceive(Level &level, const Vector3i &position, const GameEvent &event, Actor *source, float distance,
                         uint32_t range, Actor *projectileOwner) override;

    bool requiresAdjacentChunksToBeTicking() const override {
        return true;
    }

    // the circuit producer's strength: set on activation, cleared on cooldown, not saved
    int getPower() const {
        return mPower;
    }

    void setPower(int power) {
        mPower = power;
    }

    VibrationListener &listener();

protected:
    virtual uint32_t getRange() const {
        return RANGE;
    }

private:
    std::optional<VibrationListener> mListener;
    int mPower = 0;
};

class CalibratedSculkSensorBlockActor final : public SculkSensorBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "CalibratedSculkSensor";
    static constexpr uint32_t RANGE = 16;

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

protected:
    uint32_t getRange() const override {
        return RANGE;
    }
};
