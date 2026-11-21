#pragma once

struct PlayerMovementContext;

class MoveTowardsClosestSpaceSystem {
public:
    static void tick(PlayerMovementContext &context);
};
