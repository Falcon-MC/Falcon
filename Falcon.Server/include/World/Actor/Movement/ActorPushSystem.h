#pragma once

#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"

class Actor;
class ServerActor;
class ServerNetworkHandler;
struct PushableByEntityPreset;

class ActorPushSystem {
public:
    static AxisAlignedBB boundingBoxOf(const ServerActor &actor);

    static AxisAlignedBB boundingBoxOf(const Actor &actor);

    // a minecraft:pushable_by_entity ball preset adds its kick to the default push
    static Vector3f computePush(ServerNetworkHandler &owner, ServerActor &actor, const Vector3f &motion);

private:
    static void _playKickSound(ServerNetworkHandler &owner, ServerActor &actor, const PushableByEntityPreset &preset,
                               const Vector3f &kick);

    static void _accumulate(const AxisAlignedBB &self, const AxisAlignedBB &other, float &positiveX,
                            float &negativeX, float &positiveZ, float &negativeZ);
};
