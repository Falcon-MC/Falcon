#pragma once

#include <cstdint>

class ServerNetworkHandler;
class ServerPlayer;

/** A boss bar with live server numbers for one operator. Nothing is measured while the page is Off. */
class DebugHud {
public:
    enum class Page {
        Off,
        Performance,
        Generation,
        Network,
        Memory
    };

    void setPage(ServerPlayer &player, Page page);

    void tick(ServerNetworkHandler &owner, ServerPlayer &player);

private:
    Page mPage = Page::Off;
    uint32_t mBar = 0;
    uint32_t mTicksUntilRefresh = 0;
};
