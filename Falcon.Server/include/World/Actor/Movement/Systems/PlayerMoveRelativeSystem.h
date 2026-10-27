#pragma once

struct PlayerMovementContext;

class PlayerMoveRelativeSystem {
public:
    static void tick(PlayerMovementContext &context);
};
