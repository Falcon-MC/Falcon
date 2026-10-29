#pragma once

struct PlayerMovementContext;

class MovementAbilitiesSystem {
public:
    static void tick(PlayerMovementContext &context);
};
