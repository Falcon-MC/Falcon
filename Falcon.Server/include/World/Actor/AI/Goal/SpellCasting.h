#pragma once

#include <cstdint>

class MobActor;
class ServerNetworkHandler;

// the casting pose and spell particles the summon_entity and send_event goals share
namespace SpellCasting {
    void setCasting(ServerNetworkHandler &owner, MobActor &mob, bool casting);

    // ARGB, sent as actor data 77
    void sendColor(ServerNetworkHandler &owner, MobActor &mob, int32_t color);
}
