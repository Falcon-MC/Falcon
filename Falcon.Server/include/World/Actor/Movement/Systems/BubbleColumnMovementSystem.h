#pragma once

struct PlayerMovementContext;

class BubbleColumnMovementSystem {
public:
    static void tick(PlayerMovementContext &context);
};
