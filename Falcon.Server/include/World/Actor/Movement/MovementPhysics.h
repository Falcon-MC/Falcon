#pragma once

#include "World/Actor/Movement/MovementConstants.h"
#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"

class Block;
class Level;

namespace MovementPhysics {
    float mojangSin(float radians);

    float mojangCos(float radians);

    bool input(const PlayerAuthInputPacket &packet, PlayerAuthInputData flag);

    bool activeState(bool previous, const PlayerAuthInputPacket &packet,
                     PlayerAuthInputData start, PlayerAuthInputData stop);

    bool chunkLoaded(Level &level, const Vector3f &position);

    bool collides(Level &level, const AxisAlignedBB &box);

    AxisAlignedBB boxAt(const Vector3f &feet, float height);

    const Block *blockAt(Level &level, float x, float y, float z);

    void moveRelative(Vector3f &velocity, float sideways, float forward, float speed, float yawDegrees);

}
