#pragma once

#include "Core/Math/Vector3i.h"

class ServerNetworkHandler;
class ServerPlayer;

class ContainerScreenValidatorBase {
public:
    virtual ~ContainerScreenValidatorBase() = default;

    virtual bool isValid(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) const;

protected:
    static bool isWithinReach(ServerPlayer &player, const Vector3i &position);
};
