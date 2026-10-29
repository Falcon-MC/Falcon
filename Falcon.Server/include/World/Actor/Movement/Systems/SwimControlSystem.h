#pragma once

struct PlayerMovementContext;

class SwimControlSystem {
public:
    static void tick(PlayerMovementContext &context);
};
