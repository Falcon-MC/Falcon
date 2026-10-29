#pragma once

struct PlayerMovementContext;

class PlayerPredictionSystem {
public:
    static void tick(PlayerMovementContext &context);
};
