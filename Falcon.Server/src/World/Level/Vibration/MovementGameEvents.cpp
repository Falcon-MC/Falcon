#include "World/Level/Vibration/MovementGameEvents.h"

#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Actor.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"

#include <cmath>
#include <string>
#include <unordered_map>

namespace {
    struct Tracking {
        // minecraft:game_event_movement_tracking: without it an actor steps and swims but never flaps
        bool mDefined = false;
        bool mEmitMove = true;
        bool mEmitSwim = true;
        bool mEmitFlap = false;
        // minecraft:movement_sound_distance_offset, 0 without it
        float mStepOffset = 0.0f;
    };

    bool readBool(const json::Value *component, const char *name, bool fallback) {
        const json::Value *value = component == nullptr ? nullptr : component->get(name);
        return value != nullptr && value->mType == json::Value::Type::Boolean ? value->mBoolean : fallback;
    }

    Tracking readTracking(const std::string &identifier) {
        Tracking tracking;
        const json::Value *definition = EntityDefinitions::find(identifier);
        const json::Value *components = definition == nullptr ? nullptr : definition->get("components");
        if (components == nullptr)
            return tracking;

        if (const json::Value *movement = components->get("minecraft:game_event_movement_tracking")) {
            tracking.mDefined = true;
            tracking.mEmitMove = readBool(movement, "emit_move", true);
            tracking.mEmitSwim = readBool(movement, "emit_swim", true);
            tracking.mEmitFlap = readBool(movement, "emit_flap", false);
        }
        const json::Value *offset = components->get("minecraft:movement_sound_distance_offset");
        const json::Value *value = offset == nullptr ? nullptr : offset->get("value");
        if (value != nullptr && value->isNumber())
            tracking.mStepOffset = (float) value->mNumber;
        return tracking;
    }

    const Tracking &trackingOf(const Actor &actor) {
        static std::unordered_map<std::string, Tracking> cache;
        const std::string identifier = actor.getIdentifier();
        auto found = cache.find(identifier);
        if (found == cache.end())
            found = cache.emplace(identifier, readTracking(identifier)).first;
        return found->second;
    }

    bool isClimbable(const BlockState *state) {
        const Block *block = state == nullptr ? nullptr : VanillaBlocks::fromState(*state);
        return block != nullptr && block->isClimbable();
    }
}

namespace MovementGameEvents {
    // a step on the ground, climbing, in water, powder snow or a web; past the next step distance it is posted from
    // the feet. In the air nothing adds up and the last position stays: the whole fall counts once it lands
    void track(ServerNetworkHandler &owner, Actor &actor) {
        if (!actor.isAlive())
            return;
        const ServerPlayer *player = dynamic_cast<const ServerPlayer *>(&actor);
        if (player != nullptr && player->isSpectator())
            return;

        MovementGameEventState &state = actor.getMovementGameEventState();
        const Vector3f position = actor.getPosition();
        if (!state.mStarted) {
            state.mStarted = true;
            state.mLast = position;
            return;
        }

        Level &level = owner.getLevelFor(actor);
        const Tracking &tracking = trackingOf(actor);
        const int32_t x = (int32_t) std::floor(position.x);
        const int32_t y = (int32_t) std::floor(position.y);
        const int32_t z = (int32_t) std::floor(position.z);
        const BlockState *feet = level.peekBlockPtr(x, y, z);
        const bool inWater = LiquidBlocksFetch::at(level, position).water;
        const bool scaffolding = actor.getFlags().get(ActorFlag::InScaffolding)
                                 || actor.getFlags().get(ActorFlag::OverScaffolding);
        const bool trapped = feet != nullptr && (feet->mName == BlockNames::POWDER_SNOW || feet->mName == BlockNames::WEB);
        const bool climbing = scaffolding || trapped || isClimbable(feet);

        if (!inWater && !climbing && !actor.isOnGround()) {
            if (owner.getCurrentTick() < state.mNextFlapTick)
                return;
            state.mNextFlapTick = owner.getCurrentTick() + FLAP_INTERVAL_TICKS;
            if (tracking.mDefined && tracking.mEmitFlap)
                level.postGameEvent(GameEvents::FLAP, position, &actor);
            return;
        }

        const float vertical = inWater || climbing ? DISTANCE_SCALE : 0.0f;
        const float dx = (position.x - state.mLast.x) * DISTANCE_SCALE;
        const float dy = (position.y - state.mLast.y) * vertical;
        const float dz = (position.z - state.mLast.z) * DISTANCE_SCALE;
        const float moved = std::sqrt(dx * dx + dy * dy + dz * dz);
        state.mLast = position;
        if (moved <= 0.0f)
            return;
        state.mDistance += moved;
        if (state.mDistance < state.mNextStep)
            return;

        // an offset of 1 steps on whole distances
        float next = state.mDistance + tracking.mStepOffset;
        if (std::fabs(tracking.mStepOffset - 1.0f) <= 1.1920929e-7f)
            next = std::floor(next);
        state.mNextStep = next;

        const GameEvent &event = inWater ? GameEvents::SWIM : GameEvents::ENTITY_MOVE;
        if (tracking.mDefined && !(inWater ? tracking.mEmitSwim : tracking.mEmitMove))
            return;
        level.postGameEvent(event, position, &actor);
    }
}
