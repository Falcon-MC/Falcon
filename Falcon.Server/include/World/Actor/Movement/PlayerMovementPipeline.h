#pragma once

struct PlayerMovementContext;

/** Shared stage order for live inputs and catch-up replay; no alternate physics path. */
class PlayerMovementPipeline {
public:
    static void tick(PlayerMovementContext &context);
};
