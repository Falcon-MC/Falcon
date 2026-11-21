#pragma once

#include <cstdint>
#include <string>

class ServerNetworkHandler;
class ServerPlayer;
class Tag;

// the client's music manager, driven by generic level events; /music and the scripting playMusic share it
namespace MusicEvents {
    const int32_t QUEUE = 1900;
    const int32_t PLAY = 1901;
    const int32_t STOP = 1902;
    const int32_t VOLUME = 1903;

    // what queue and play carry
    Tag trackTag(const std::string &trackName, float volume, float fadeSeconds, bool loop);

    // player null: every spawned player
    void send(ServerNetworkHandler &owner, ServerPlayer *player, int32_t event, const Tag &data);
}
