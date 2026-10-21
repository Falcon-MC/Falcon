#pragma once

struct PlayerMovementContext;

class CurrentSwimAmountSystem {
public:
    static void tick(PlayerMovementContext &context);
};
