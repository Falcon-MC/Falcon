#include "Command/CommandOrigin.h"

#include "Actor/ServerPlayer.h"

Vector3f CommandOrigin::getPosition() {
    const ServerPlayer *player = asPlayer();
    return player == nullptr ? Vector3f(0.0f, 0.0f, 0.0f) : player->getPosition();
}

Vector3f CommandOrigin::getRotation() {
    const ServerPlayer *player = asPlayer();
    return player == nullptr ? Vector3f(0.0f, 0.0f, 0.0f) : player->getRotation();
}

Level *CommandOrigin::getLevel() {
    return nullptr;
}
