#pragma once

#include "World/Actor/AI/Goal/Goal.h"

#include <cstdint>

class MobActor;
class ServerNetworkHandler;

// the casting pose and spell particles the summon_entity and send_event goals share
namespace SpellCasting {
    // the caster stands still and faces its target until the cast ends, then may move while the rest spawns
    const uint8_t CASTING_FLAGS = (uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look
                                  | (uint8_t) GoalControlFlag::Spell;
    const uint8_t AFTER_CAST_FLAGS = (uint8_t) GoalControlFlag::Spell;

    void setCasting(ServerNetworkHandler &owner, MobActor &mob, bool casting);

    // ARGB, sent as actor data 77
    void sendColor(ServerNetworkHandler &owner, MobActor &mob, int32_t color);
}
