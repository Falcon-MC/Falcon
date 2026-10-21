#pragma once

struct PlayerMovementContext;

class PlayerTravelPreparationSystem {
public:
    static void tick(PlayerMovementContext &context);
};
