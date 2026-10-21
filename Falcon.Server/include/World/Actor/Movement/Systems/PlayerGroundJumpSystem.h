#pragma once

struct PlayerMovementContext;

class PlayerGroundJumpSystem {
public:
    static void tick(PlayerMovementContext &context);
};
