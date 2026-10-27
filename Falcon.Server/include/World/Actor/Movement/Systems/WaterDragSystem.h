#pragma once

struct PlayerMovementContext;

class WaterDragSystem {
public:
    static void tick(PlayerMovementContext &context);
};
