#pragma once

#include <cstdint>
struct MovementAbilitiesComponent {
    int32_t gameType = 0;
    bool spectator = false;
    bool flying = false;
    // the vertical fly controls follow the flight state of the input itself: on from the input that starts flight,
    // off from the one that stops it, while the travel keeps the previous state for that input
    bool flightControls = false;
    bool startsFlight = false;
    bool gliding = false;
    bool swimming = false;
    bool sprinting = false;
};
