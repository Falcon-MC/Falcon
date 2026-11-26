#pragma once

#include <cstdint>
#include <string>

// how a piston treats a block: the game's movable block component
enum class PistonMovement : uint8_t {
    // pushed, and pulled by a sticky piston or a slime or honey block
    PushPull,
    // pushed, never pulled
    Push,
    // broken when pushed, left in place when pulled
    Popped,
    // stops a push, left in place when pulled
    Immovable
};

struct PistonMovable {
    PistonMovement mMovement = PistonMovement::PushPull;
    // slime and honey: move their neighbours along, except blocks of the other sticky kind
    bool mSticky = false;

    // blocks the game gives no movable component are pushed and pulled
    static PistonMovable of(const std::string &identifier);
};
