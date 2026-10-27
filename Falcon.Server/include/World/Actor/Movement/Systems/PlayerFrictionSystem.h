#pragma once

struct PlayerMovementContext;

class PlayerFrictionSystem {
public:
    static void tick(PlayerMovementContext &context);
};
