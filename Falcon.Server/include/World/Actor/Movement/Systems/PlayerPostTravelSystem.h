#pragma once

struct PlayerMovementContext;

class PlayerPostTravelSystem {
public:
    static void tick(PlayerMovementContext &context);
};
