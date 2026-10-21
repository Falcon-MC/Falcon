#pragma once

struct PlayerMovementContext;

class PlayerMovementInitializationSystem {
public:
    static void tick(PlayerMovementContext &context);
};
