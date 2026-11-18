#include "World/Level/Block/Actor/BeaconBlockActor.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockLightProperties.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Level.h"

#include <cmath>
#include <initializer_list>

namespace {
    const char *const TAG_PRIMARY = "primary";
    const char *const TAG_SECONDARY = "secondary";
    // written by older builds
    const char *const TAG_OLD_PRIMARY = "Primary";
    const char *const TAG_OLD_SECONDARY = "Secondary";

    const char *const UNKNOWN = "minecraft:unknown";

    // one list per pyramid level: a level offers its own effects and those below it
    const std::initializer_list<MobEffectId> TIERS[BeaconBlockActor::MAX_LEVELS] = {
            {MobEffectId::Speed, MobEffectId::Haste},
            {MobEffectId::Resistance, MobEffectId::JumpBoost},
            {MobEffectId::Strength},
            {MobEffectId::Regeneration}};

    // how many effects each level gives: the secondary only at the top
    const int32_t EFFECT_COUNTS[BeaconBlockActor::MAX_LEVELS] = {1, 1, 1, 2};

    const float RANGES[BeaconBlockActor::MAX_LEVELS] = {20.0f, 30.0f, 40.0f, 50.0f};

    bool isBaseBlock(const std::string &name) {
        return name == BlockNames::IRON_BLOCK || name == BlockNames::GOLD_BLOCK || name == BlockNames::EMERALD_BLOCK
               || name == BlockNames::DIAMOND_BLOCK || name == BlockNames::NETHERITE_BLOCK;
    }

    int32_t readEffect(const Tag &data, const char *key, const char *oldKey) {
        if (data.get(key) != nullptr)
            return data.getInt(key);
        return data.getInt(oldKey, 0);
    }
}

void BeaconBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;
    mPrimaryEffect = readEffect(data, TAG_PRIMARY, TAG_OLD_PRIMARY);
    mSecondaryEffect = readEffect(data, TAG_SECONDARY, TAG_OLD_SECONDARY);
}

Tag BeaconBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    data.putInt(TAG_PRIMARY, mPrimaryEffect);
    data.putInt(TAG_SECONDARY, mSecondaryEffect);
    return data;
}

Tag BeaconBlockActor::getSpawnCompound() const {
    // the screen shows the chosen effects from these
    Tag data = BlockActor::getSpawnCompound();
    data.putInt(TAG_PRIMARY, mPrimaryEffect);
    data.putInt(TAG_SECONDARY, mSecondaryEffect);
    return data;
}

bool BeaconBlockActor::isEffectAvailable(int32_t effect, int32_t levels) {
    if (effect <= 0)
        return false;
    for (int32_t tier = 0; tier < levels && tier < MAX_LEVELS; ++tier) {
        for (const MobEffectId id: TIERS[tier]) {
            if ((int32_t) id == effect)
                return true;
        }
    }
    return false;
}

void BeaconBlockActor::applyPayment(int32_t primary, int32_t secondary) {
    if (primary > 0 && isEffectAvailable(primary, mLevels))
        mPrimaryEffect = primary;
    if (secondary <= 0)
        return;

    // the secondary is regeneration or a second level of the primary
    const bool allowed = isEffectAvailable(secondary, mLevels)
                         && (secondary == (int32_t) MobEffectId::Regeneration || secondary == mPrimaryEffect);
    mSecondaryEffect = allowed ? secondary : 0;
}

bool BeaconBlockActor::isPaymentItem(const std::string &identifier) {
    // the game's beacon_payment_items tag
    return identifier == "minecraft:iron_ingot" || identifier == "minecraft:gold_ingot"
           || identifier == "minecraft:emerald" || identifier == "minecraft:diamond"
           || identifier == "minecraft:netherite_ingot";
}

bool BeaconBlockActor::measurePyramid(Level &level, const Vector3i &position, int32_t &outLevels) {
    for (int32_t tier = 1; tier <= MAX_LEVELS; ++tier) {
        const int32_t y = position.y - tier;
        for (int32_t x = position.x - tier; x <= position.x + tier; ++x) {
            for (int32_t z = position.z - tier; z <= position.z + tier; ++z) {
                const BlockState *state = level.peekBlockPtr(x, y, z);
                if (state != nullptr && isBaseBlock(state->mName))
                    continue;

                outLevels = tier - 1;
                return state != nullptr && state->mName != UNKNOWN;
            }
        }
    }
    outLevels = MAX_LEVELS;
    return true;
}

bool BeaconBlockActor::measureBeam(Level &level, const Vector3i &position) {
    for (int32_t y = position.y + 1; y <= level.getMaxY(); ++y) {
        const BlockState *state = level.peekBlockPtr(position.x, y, position.z);
        if (state == nullptr || state->mName == UNKNOWN)
            return false;
        // bedrock is opaque but lets the beam through
        if (state->mName != BlockNames::BEDROCK
            && BlockLightProperties::lightFilter(BlockLightProperties::packed(*state)) >= 15)
            return false;
    }
    return true;
}

int32_t BeaconBlockActor::effectsFor(int32_t levels, int32_t primary, int32_t secondary, Effect (&outEffects)[2]) {
    if (levels <= 0 || levels > MAX_LEVELS)
        return 0;

    int32_t count = 0;
    if (isEffectAvailable(primary, levels))
        outEffects[count++] = Effect{primary, 0};
    if (EFFECT_COUNTS[levels - 1] != 1 && isEffectAvailable(secondary, levels)
        && (secondary == (int32_t) MobEffectId::Regeneration || secondary == primary))
        outEffects[count++] = Effect{secondary, 0};

    // the same effect twice is one effect a level higher, at the top of the pyramid
    if (count == 2 && outEffects[0].mId == outEffects[1].mId) {
        count = 1;
        outEffects[0].mAmplifier = levels == MAX_LEVELS ? 1 : 0;
    }
    return count;
}

float BeaconBlockActor::rangeFor(int32_t levels) {
    if (levels <= 0)
        return 0.0f;
    return RANGES[(levels > MAX_LEVELS ? MAX_LEVELS : levels) - 1] + 1.0f;
}

bool BeaconBlockActor::tick(ServerNetworkHandler &owner) {
    if (mLevel == nullptr)
        return true;

    Level &level = *mLevel;
    const BlockState *state = level.peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    if (state == nullptr)
        return true;
    if (state->mName != BlockNames::BEACON)
        return false;

    // the sounds play at the block's corner, as the game does
    const Vector3f corner((float) mPosition.x, (float) mPosition.y, (float) mPosition.z);
    if (level.getWorldTick() % UPDATE_TICKS == 0 && mLevels > 0)
        owner.playLevelSound(level, "beacon.ambient", corner);

    if (mCountdown-- > 1)
        return true;

    mCountdown = UPDATE_TICKS;
    _update(owner, level);
    return true;
}

void BeaconBlockActor::_update(ServerNetworkHandler &owner, Level &level) {
    const int32_t previous = mLevels;
    const bool complete = measurePyramid(level, mPosition, mLevels);

    if ((previous > 0) != (mLevels > 0)) {
        const Vector3f corner((float) mPosition.x, (float) mPosition.y, (float) mPosition.z);
        owner.playLevelSound(level, mLevels > 0 ? "beacon.activate" : "beacon.deactivate", corner);
    }

    mBeam = complete && mLevels > 0 && measureBeam(level, mPosition);
    if (mBeam)
        _applyEffects(owner, level);
}

void BeaconBlockActor::_applyEffects(ServerNetworkHandler &owner, Level &level) {
    Effect effects[2];
    const int32_t count = effectsFor(mLevels, mPrimaryEffect, mSecondaryEffect, effects);
    if (count == 0)
        return;

    const float range = rangeFor(mLevels);
    const Vector3f corner((float) mPosition.x, (float) mPosition.y, (float) mPosition.z);
    const int32_t duration = durationFor(mLevels);

    owner.forEachPlayerNear(corner, range, [&](ServerPlayer &player) {
        if (!player.isSpawned() || player.isDead() || player.isSpectator() || !player.isIn(level))
            return;

        // the eyes count, inside the horizontal square and above its floor; no ceiling
        const Vector3f &feet = player.getPosition();
        const float eyes = feet.y + MovementPhysics::PLAYER_BASE_OFFSET;
        if (feet.x <= corner.x - range || feet.x >= corner.x + range || eyes <= corner.y - range
            || feet.z <= corner.z - range || feet.z >= corner.z + range)
            return;

        for (int32_t index = 0; index < count; ++index) {
            MobEffectInstance instance;
            instance.mId = (MobEffectId) effects[index].mId;
            instance.mAmplifier = effects[index].mAmplifier;
            instance.mDuration = duration;
            instance.mAmbient = true;
            owner.addEffect(player, instance);
        }
    });
}
