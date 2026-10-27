#pragma once

struct PlayerMovementContext;

class UpdateHorizontalPoseSystem {
public:
    static void tick(PlayerMovementContext &context);
};
