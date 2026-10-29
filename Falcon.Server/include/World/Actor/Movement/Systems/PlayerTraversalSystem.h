#pragma once

struct PlayerMovementContext;

class PlayerTraversalSystem {
public:
    static void tick(PlayerMovementContext &context);
};
