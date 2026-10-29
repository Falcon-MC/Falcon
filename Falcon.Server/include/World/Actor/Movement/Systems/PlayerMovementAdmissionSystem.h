#pragma once

struct PlayerMovementContext;

class PlayerMovementAdmissionSystem {
public:
    static bool prepare(PlayerMovementContext &context);
};
