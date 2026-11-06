#pragma once

#include "Protocol/Packets/BossEventPacket.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class ServerNetworkHandler;
class ServerPlayer;

/**
 * The boss bars the server and plugins show to one player. The client draws one bar per boss actor and the player is
 * the only actor every client knows, so the newest bar is displayed and the others wait behind it. A raid bar uses the
 * same actor and takes the place of all of them while it is up.
 */
class PlayerBossBars {
public:
    // wire values of the client's BossBarColor (0x147E1D5A0)
    enum class Color : int32_t {
        Pink,
        Blue,
        Red,
        Green,
        Yellow,
        Purple,
        RebeccaPurple,
        White
    };

    // the id of the new bar; plugins may call this from any thread
    uint32_t add(const std::string &title, float percent, Color color);

    bool update(uint32_t id, const std::string &title, float percent, Color color);

    bool remove(uint32_t id);

    void tick(ServerNetworkHandler &owner, ServerPlayer &player);

private:
    struct Bar {
        uint32_t mId = 0;
        std::string mTitle;
        float mPercent = 1.0f;
        Color mColor = Color::Purple;
    };

    void _send(ServerNetworkHandler &owner, ServerPlayer &player, BossEventPacket::Action action, const Bar &bar);

    std::mutex mMutex;
    std::vector<Bar> mBars;
    uint32_t mNextId = 1;
    bool mShown = false;
    Bar mDisplayed;
};
