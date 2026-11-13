#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Core/NBT/Tag.h"
#include "World/Level/Vibration/GameEvent.h"
#include "World/Level/Vibration/GameEventListener.h"

#include <cstdint>
#include <optional>

class Actor;
class Level;

// what a block or an entity does with the vibrations its listener picks (VibrationListenerConfig)
class VibrationListenerConfig {
public:
    virtual ~VibrationListenerConfig() = default;

    virtual bool canReceive(const GameEvent &event) const {
        return event.has(GameEvent::VIBRATION);
    }

    virtual bool shouldListen(Level &level, const GameEvent &event, const GameEventContext &context) = 0;

    // position: where the vibration came from; distance: whole blocks, as it travelled
    virtual void onSignalReceive(Level &level, const Vector3i &position, const GameEvent &event, Actor *source,
                                 float distance, uint32_t range, Actor *projectileOwner) = 0;

    // the signal waits at the listener until the 3x3 chunks around it tick
    virtual bool requiresAdjacentChunksToBeTicking() const {
        return false;
    }
};

// one vibration: the candidate of a tick or the signal on its way
struct VibrationInfo {
    Vector3i mPosition;
    float mDistance = 0.0f;
    const GameEvent *mEvent = &GameEvents::INVALID;
    std::optional<int64_t> mSource;
    std::optional<int64_t> mProjectileOwner;

    Tag save() const;

    static VibrationInfo load(const Tag &data);
};

// VibrationListener of a block: picks the closest vibration of a tick, sends it on, delivers it distance ticks later
class VibrationListener final : public GameEventListener {
public:
    static constexpr float SIGNAL_SPEED = 20.0f;

    VibrationListener(VibrationListenerConfig &config, const Vector3i &blockPosition, uint32_t range);

    void handleGameEvent(Level &level, const GameEvent &event, const GameEventContext &context) override;

    bool getPosition(Level &level, Vector3f &position) const override;

    uint32_t getRange() const override {
        return mRange;
    }

    void tick(Level &level);

    const GameEvent &getLastEvent() const {
        return *mLastEvent;
    }

    bool hasPendingVibration() const {
        return mPending.has_value();
    }

    Tag save() const;

    void load(const Tag &data);

private:
    void _select(int64_t tick, const GameEvent &event, const GameEventContext &context);

    bool _adjacentChunksTicking(Level &level) const;

    VibrationListenerConfig &mConfig;
    Vector3i mBlockPosition;
    Vector3f mCenter;
    uint32_t mRange;
    // VibrationSelector: the tick its candidate was heard on, -1 for none
    int64_t mSelectorTick = -1;
    std::optional<VibrationInfo> mCandidate;
    std::optional<VibrationInfo> mPending;
    int32_t mTravelTicks = 0;
    const GameEvent *mLastEvent = &GameEvents::INVALID;
};
