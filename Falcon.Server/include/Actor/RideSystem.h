#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Types/EntityLinkData.h"

#include <cstdint>
#include <vector>

class Actor;
class ServerActor;
class ServerNetworkHandler;

class RideSystem {
public:
    static bool mount(ServerNetworkHandler &owner, Actor &rider, ServerActor &vehicle, bool riderInitiated);

    static void dismount(ServerNetworkHandler &owner, Actor &rider, bool riderInitiated);

    static void ejectAll(ServerNetworkHandler &owner, Actor &vehicle);

    static void appendLinks(const Actor &actor, std::vector<EntityLinkData> &links);

    static Actor *resolve(ServerNetworkHandler &owner, int64_t uniqueId);

    static void syncPassengerPositions(ServerNetworkHandler &owner, Actor &vehicle);
};
