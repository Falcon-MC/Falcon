#pragma once

struct PlayerMovementContext;

class PlayerFlightSystem {
public:
    static void tick(PlayerMovementContext &context);
};
