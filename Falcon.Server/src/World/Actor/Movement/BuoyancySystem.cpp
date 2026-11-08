#include "World/Actor/Movement/BuoyancySystem.h"

#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>

namespace {
    const float SUBMERGED_SHIFT = 0.1f;
    const float BUOYANCY_SCALE = 0.9f;
    const float VERTICAL_DRAG = 0.7f;
    const float LIFT = 0.15f;
    const float MAX_RISE_PER_TICK = 0.05f;
    const float LEASHED_KEPT_SPEED = 0.05f;
    const float WAVE_AMPLITUDE = 0.035f;
    const float BOBBING_AMPLITUDE = 0.5f;
    const float BOBBING_OFFSET = 1.2f;
    const double BOBBING_PHASE = 0.3141592741012573;
    const double TICK_SECONDS = 0.05000000074505806;
    const float WAVE_SPEED_SCALE = 30.0f;

    float numberIn(const json::Value &component, const char *key, float fallback) {
        const json::Value *value = component.get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }
}

BuoyancySettings BuoyancySystem::read(const json::Value *component) {
    BuoyancySettings settings;
    if (component == nullptr)
        return settings;

    settings.mEnabled = true;
    settings.mBaseBuoyancy = numberIn(*component, "base_buoyancy", settings.mBaseBuoyancy);
    settings.mBigWaveProbability = numberIn(*component, "big_wave_probability", settings.mBigWaveProbability);
    settings.mBigWaveSpeed = numberIn(*component, "big_wave_speed", settings.mBigWaveSpeed);

    const json::Value *gravity = component->get("apply_gravity");
    settings.mApplyGravity = gravity == nullptr || gravity->boolean(true);

    const json::Value *waves = component->get("simulate_waves");
    settings.mMovement = waves == nullptr || waves->boolean(true) ? BuoyancyMovement::Waves : BuoyancyMovement::None;
    const json::Value *movement = component->get("movement_type");
    if (movement != nullptr && movement->isString()) {
        if (movement->mString == "bobbing")
            settings.mMovement = BuoyancyMovement::Bobbing;
        else if (movement->mString == "waves")
            settings.mMovement = BuoyancyMovement::Waves;
        else if (movement->mString == "none")
            settings.mMovement = BuoyancyMovement::None;
    }

    const json::Value *liquids = component->get("liquid_blocks");
    if (liquids != nullptr && liquids->isArray()) {
        settings.mWater = false;
        for (const std::unique_ptr<json::Value> &liquid: liquids->mArray) {
            const std::string &name = liquid->string();
            settings.mWater = settings.mWater || name == "minecraft:water" || name == "minecraft:flowing_water";
            settings.mLava = settings.mLava || name == "minecraft:lava" || name == "minecraft:flowing_lava";
        }
    }
    return settings;
}

BuoyancyContact BuoyancySystem::contact(Level &level, const Vector3f &position, const BuoyancySettings &settings) {
    BuoyancyContact result;
    if (!settings.mEnabled)
        return result;

    const int32_t x = (int32_t) std::floor(position.x);
    const int32_t y = (int32_t) std::floor(position.y);
    const int32_t z = (int32_t) std::floor(position.z);
    const BlockState *feet = level.peekBlockPtr(x, y, z);
    if (feet == nullptr || !_isFloatLiquid(settings, *feet))
        return result;

    const BlockState *above = level.peekBlockPtr(x, y + 1, z);
    const bool liquidAbove = above != nullptr && _isFloatLiquid(settings, *above);
    result.mSubmerged = liquidAbove;
    result.mSurface = !liquidAbove && position.y < (float) y + 1.0f - LiquidView(*feet).getFluidHeightPercent();
    return result;
}

bool BuoyancySystem::_isFloatLiquid(const BuoyancySettings &settings, const BlockState &state) {
    const LiquidView liquid(state);
    return (settings.mWater && liquid.isWater()) || (settings.mLava && liquid.isLava());
}

double BuoyancySystem::advanceTimer(double timer, const BuoyancySettings &settings, const Vector3f &velocity,
                                    std::mt19937 &random) {
    // sub_14695ACE0
    if (settings.mMovement == BuoyancyMovement::Bobbing)
        return timer + 1.0;

    float speed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z) * WAVE_SPEED_SCALE + 1.0f;
    if (settings.mBigWaveProbability >= std::uniform_real_distribution<float>(0.0f, 1.0f)(random))
        speed *= settings.mBigWaveSpeed;
    return timer + speed * TICK_SECONDS;
}

float BuoyancySystem::floatVelocity(float velocityY, float positionY, double timer, const BuoyancySettings &settings,
                                    const BuoyancyContact &contact, bool leashed) {
    // sub_146959470: under the surface the lift is full and the waves do not count
    float buoyancy = 1.0f;
    float wave = 0.0f;
    if (contact.mSurface) {
        const float submerged = positionY - std::floor(positionY);
        buoyancy = std::clamp((settings.mBaseBuoyancy - submerged) * BUOYANCY_SCALE + SUBMERGED_SHIFT, 0.0f, 1.0f);
        if (settings.mMovement == BuoyancyMovement::Waves)
            wave = ((float) std::sin(timer) + 1.0f) * WAVE_AMPLITUDE;
        else if (settings.mMovement == BuoyancyMovement::Bobbing)
            wave = ((float) std::sin(timer * BOBBING_PHASE) + BOBBING_OFFSET) * BOBBING_AMPLITUDE;
    } else if (!contact.mSubmerged) {
        return velocityY;
    }
    if (buoyancy == 0.0f)
        return velocityY;

    const float dragged = velocityY * VERTICAL_DRAG;
    const float target = (buoyancy - wave - SUBMERGED_SHIFT) * LIFT;
    if (leashed && dragged > 0.0f)
        return std::max(target, dragged * LEASHED_KEPT_SPEED);
    return std::min(target, dragged + MAX_RISE_PER_TICK);
}
