#pragma once

#include "World/Level/Block/Actor/BlockActor.h"
#include "World/Level/Sculk/SculkSpreader.h"
#include "World/Level/Vibration/GameEventListener.h"

#include <random>

// SculkCatalystBlockActor: hears what dies near it and spreads sculk with the experience it would have dropped
class SculkCatalystBlockActor final : public BlockActor, public GameEventListener {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "SculkCatalyst";
    static constexpr uint32_t RANGE = 10;
    // the bloom shows this long
    static constexpr int32_t BLOOM_TICKS = 8;

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    bool tick(ServerNetworkHandler &owner) override;

    GameEventListener *getGameEventListener() override {
        return this;
    }

    void handleGameEvent(Level &level, const GameEvent &event, const GameEventContext &context) override;

    bool getPosition(Level &level, Vector3f &position) const override;

    uint32_t getRange() const override {
        return RANGE;
    }

    SculkSpreader &getSpreader() {
        return mSpreader;
    }

    // the catalyst pulses: bloom on, a scheduled end and the soul particles
    void bloom(Level &level);

private:
    SculkSpreader mSpreader;
    std::mt19937 mRandom{std::random_device{}()};
};
