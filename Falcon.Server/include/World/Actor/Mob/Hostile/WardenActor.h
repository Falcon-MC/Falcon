#pragma once

#include "World/Actor/Mob/Hostile/HostileActor.h"
#include "World/Level/Vibration/VibrationListener.h"

#include <memory>
#include <optional>

class WardenActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:warden";
    // ticks between two vibrations it listens to
    static constexpr uint16_t VIBRATION_COOLDOWN = 40;
    static constexpr uint32_t VIBRATION_RANGE = 16;
    // a projectile owner farther than this is not blamed for its vibration
    static constexpr float PROJECTILE_OWNER_RANGE = 30.0f;
    // a projectile vibration this soon after the last one points at its owner
    static constexpr int64_t RECENT_PROJECTILE_TICKS = 100;
    static constexpr int64_t TOUCH_COOLDOWN = 20;

    using HostileActor::HostileActor;

    ActorSize getSize() const override {
        return ActorSize{0.9f, 2.9f};
    }

    float getDefaultMaxHealth() const override {
        return 500.0f;
    }

    bool preventsSleep() const override {
        return false;
    }

    void tick(ServerNetworkHandler &owner) override;

    void onDamaged(ServerNetworkHandler &owner, Actor *attacker) override;

    void onPushedBy(ServerNetworkHandler &owner, int64_t pusherId) override;

    GameEventListener *getGameEventListener() override;

    // the block of the last vibration it received, and when
    const std::optional<Vector3i> &getLastVibration() const {
        return mLastVibration;
    }

    int64_t getLastVibrationTick() const {
        return mLastVibrationTick;
    }

    void receiveVibration(ServerNetworkHandler &owner, const Vector3i &position, Actor *source,
                          Actor *projectileOwner);

    bool listensTo(ServerNetworkHandler &owner, const GameEventContext &context);

private:
    class VibrationConfig final : public VibrationListenerConfig {
    public:
        explicit VibrationConfig(WardenActor &warden) : mWarden(warden) {}

        bool canReceive(const GameEvent &event) const override;

        bool shouldListen(Level &level, const GameEvent &event, const GameEventContext &context) override;

        void onSignalReceive(Level &level, const Vector3i &position, const GameEvent &event, Actor *source,
                             float distance, uint32_t range, Actor *projectileOwner) override;

    private:
        WardenActor &mWarden;
    };

    VibrationListener &_listener();

    VibrationConfig mVibrationConfig{*this};
    std::unique_ptr<VibrationListener> mListener;
    std::optional<int64_t> mLastListenTick;
    int64_t mLastProjectileTick = 0;
    int64_t mNextTouchTick = 0;
    std::optional<Vector3i> mLastVibration;
    int64_t mLastVibrationTick = 0;
};
