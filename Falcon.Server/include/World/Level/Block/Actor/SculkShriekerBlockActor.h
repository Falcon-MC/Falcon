#pragma once

#include "World/Level/Block/Actor/BlockActor.h"
#include "World/Level/Vibration/VibrationListener.h"

#include <cstdint>
#include <optional>

class ServerPlayer;

// SculkShriekerBlockActor: the shrieker's listener and the warning it answers with once the shriek ends
class SculkShriekerBlockActor : public BlockActor, public VibrationListenerConfig {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "SculkShrieker";
    static constexpr uint32_t RANGE = 8;
    // the shriek lasts this long before the shrieker answers
    static constexpr int32_t SHRIEK_TICKS = 90;
    // players this close to the shrieker share one warning level
    static constexpr float WARNING_RANGE = 16.0f;
    // no shriek while a warden is this close on any axis
    static constexpr int32_t NEARBY_WARDEN_RANGE = 24;
    static constexpr float DARKNESS_RANGE = 40.0f;
    static constexpr int32_t DARKNESS_TICKS = 260;

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    bool tick(ServerNetworkHandler &owner) override;

    GameEventListener *getGameEventListener() override {
        return &listener();
    }

    bool canReceive(const GameEvent &event) const override;

    bool shouldListen(Level &level, const GameEvent &event, const GameEventContext &context) override;

    void onSignalReceive(Level &level, const Vector3i &position, const GameEvent &event, Actor *source, float distance,
                         uint32_t range, Actor *projectileOwner) override;

    VibrationListener &listener();

    // the player behind an actor: itself, the one steering it, or the one who shot it
    static ServerPlayer *responsiblePlayer(ServerNetworkHandler &owner, Actor *actor);

    // warns the players near it through player; false when it did not shriek
    bool tryShriek(ServerNetworkHandler &owner, Level &level, ServerPlayer &player);

    // the shriek is over: a warden, or a reply from the deep
    void respond(ServerNetworkHandler &owner, Level &level);

    const std::optional<int32_t> &getWarningLevel() const {
        return mWarningLevel;
    }

private:
    void _shriek(ServerNetworkHandler &owner, Level &level, ServerPlayer &player);

    bool _canRespond(ServerNetworkHandler &owner, Level &level) const;

    bool _tryWarn(ServerNetworkHandler &owner, Level &level, ServerPlayer &player);

    bool _summonWarden(ServerNetworkHandler &owner, Level &level);

    std::optional<VibrationListener> mListener;
    std::optional<int32_t> mWarningLevel;
};
