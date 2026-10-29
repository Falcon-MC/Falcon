#pragma once

struct PlayerMovementContext;

class GlideMoveSystem {
public:
    static void tick(PlayerMovementContext &context);
};
