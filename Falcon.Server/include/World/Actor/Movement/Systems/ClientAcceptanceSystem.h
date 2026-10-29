#pragma once

struct PlayerMovementContext;

class ClientAcceptanceSystem {
public:
    static void tick(PlayerMovementContext &context);
};
