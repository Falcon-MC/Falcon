#include "World/Actor/Definition/KnockbackRules.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "Scripting/Binding/ScriptRaycast.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Level.h"

#include <cmath>
#include <memory>

namespace {
    const char *const APPLY_KNOCKBACK_RULES_COMPONENT = "minecraft:apply_knockback_rules";

    float numberOr(const json::Value &preset, const char *key, float fallback) {
        const json::Value *value = preset.get(key);
        return value != nullptr && value->isNumber() ? (float) value->number() : fallback;
    }

    bool isWaterAt(Level &level, int32_t x, int32_t y, int32_t z) {
        for (int layer = 0; layer < 2; ++layer) {
            const BlockState *state = level.peekBlockPtr(x, y, z, layer);
            if (state != nullptr && LiquidView(*state).isWater())
                return true;
        }
        return false;
    }
}

bool KnockbackRules::select(ServerNetworkHandler &owner, const MobActor &attacker, const Actor &target,
                            KnockbackPreset &preset) {
    const json::Value *rules = attacker.getComponent(APPLY_KNOCKBACK_RULES_COMPONENT);
    const json::Value *presets = rules == nullptr ? nullptr : rules->get("presets");
    if (presets == nullptr || !presets->isArray())
        return false;

    for (const std::unique_ptr<json::Value> &entry: presets->mArray) {
        if (entry == nullptr || !entry->isObject())
            continue;

        const json::Value *filter = entry->get("filter");
        if (filter != nullptr && !EntityFilter::test(*filter, owner, attacker, &target))
            continue;

        // the first matching preset decides: one asking for a target in water falls back to the usual knockback
        const json::Value *immersed = entry->get("check_if_target_is_immersed_in_water");
        if (immersed != nullptr && immersed->boolean(false) && !isImmersedInWater(owner, target))
            return false;

        preset.mHorizontalPower = numberOr(*entry, "horizontal_power", preset.mHorizontalPower);
        preset.mVerticalPower = numberOr(*entry, "vertical_power", preset.mVerticalPower);
        preset.mVerticalVelocityCap = numberOr(*entry, "vertical_velocity_cap", preset.mVerticalVelocityCap);
        preset.mSlowdownScale = numberOr(*entry, "slowdown_scale", preset.mSlowdownScale);
        return true;
    }
    return false;
}

bool KnockbackRules::isImmersedInWater(ServerNetworkHandler &owner, const Actor &actor) {
    Level &level = owner.getLevelFor(actor);
    // the box of the current pose: a swimming player is 0.6 high
    const ActorSize size = ScriptRaycast::actorSize(actor);
    const Vector3f position = actor.getPosition();
    const int32_t x = (int32_t) std::floor(position.x);
    const int32_t z = (int32_t) std::floor(position.z);
    return isWaterAt(level, x, (int32_t) std::floor(position.y + size.mHeight), z)
           && isWaterAt(level, x, (int32_t) std::floor(position.y), z);
}
