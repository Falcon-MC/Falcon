#pragma once

struct PlayerMovementContext;

class PlayerFlightDragSystem {
public:
    static void tick(PlayerMovementContext &context);
};
