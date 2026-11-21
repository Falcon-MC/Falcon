#pragma once

#include "World/Level/Block/Actor/BlockActor.h"

#include <cstdint>
#include <string>

class Level;

/**
 * The block actor of a beacon. Every 80 ticks it measures its pyramid and its beam, then gives its chosen effects
 * to the players in range. The payment slot lives in the player inventory: the beacon itself keeps no items.
 */
class BeaconBlockActor final : public BlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Beacon";

    static constexpr int32_t MAX_LEVELS = 4;

    static constexpr int32_t UPDATE_TICKS = 80;

    const char *getBlockActorId() const override {
        return BLOCK_ACTOR_ID;
    }

    Tag saveNbt() const override;

    Tag getSpawnCompound() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    bool tick(ServerNetworkHandler &owner) override;

    int32_t getLevels() const noexcept {
        return mLevels;
    }

    int32_t getPrimaryEffect() const noexcept {
        return mPrimaryEffect;
    }

    int32_t getSecondaryEffect() const noexcept {
        return mSecondaryEffect;
    }

    // a beam blocked by an opaque block, or a pyramid next to unloaded chunks, gives nothing
    bool hasBeam() const noexcept {
        return mBeam;
    }

    // whether a pyramid of that many levels offers the effect
    static bool isEffectAvailable(int32_t effect, int32_t levels);

    // the payment's choice, each effect kept only where the pyramid allows it; 0 keeps the current one
    void applyPayment(int32_t primary, int32_t secondary);

    static bool isPaymentItem(const std::string &identifier);

    // the pyramid levels under the beacon, 0 to 4; false when a layer reaches into unloaded chunks
    static bool measurePyramid(Level &level, const Vector3i &position, int32_t &outLevels);

    // whether nothing opaque stands between the beacon and the build limit
    static bool measureBeam(Level &level, const Vector3i &position);

    // the effects the players in range get, amplifier and all; empty while nothing is chosen
    struct Effect {
        int32_t mId = 0;
        int32_t mAmplifier = 0;
    };

    static int32_t effectsFor(int32_t levels, int32_t primary, int32_t secondary, Effect (&outEffects)[2]);

    static float rangeFor(int32_t levels);

    static int32_t durationFor(int32_t levels) {
        return 40 * levels + 180;
    }

private:
    void _update(ServerNetworkHandler &owner, Level &level);

    void _applyEffects(ServerNetworkHandler &owner, Level &level);

    int32_t mPrimaryEffect = 0;
    int32_t mSecondaryEffect = 0;
    int32_t mLevels = 0;
    int32_t mCountdown = UPDATE_TICKS;
    bool mBeam = false;
};
