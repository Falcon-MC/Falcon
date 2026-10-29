#pragma once

#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"

class Actor;
class ServerActor;
class ServerNetworkHandler;

class ActorPushSystem {
public:
    static AxisAlignedBB boundingBoxOf(const ServerActor &actor);

    static AxisAlignedBB boundingBoxOf(const Actor &actor);

    static Vector3f computePush(ServerNetworkHandler &owner, const ServerActor &actor, const Vector3f &motion);

private:
    static void _accumulate(const AxisAlignedBB &self, const AxisAlignedBB &other, float &positiveX,
                            float &negativeX, float &positiveZ, float &negativeZ);
};
