#include "Actor/AI/Control/BodyControl.h"

#include "Actor/AI/Control/LookControl.h"
#include "Actor/AI/Control/MoveControl.h"
#include "Actor/Mob/MobActor.h"

#include <algorithm>
#include <cmath>

void BodyControl::tick(MobActor &mob, const MoveControl &moveControl, const LookControl &lookControl) {
    if (!moveControl.hasWanted())
        return;

    Vector3f rotation = mob.getRotation();
    const float wanted = LookControl::yawTowards(mob.getPosition(), moveControl.getWantedPosition());
    const float maxTurn = mob.getMaxTurn();
    const float turn = std::max(-maxTurn, std::min(maxTurn, std::remainder(wanted - rotation.y, 360.0f)));
    rotation.y = std::fmod(rotation.y + turn + 360.0f, 360.0f);
    if (!lookControl.hasLookAt())
        rotation.z = rotation.y;
    mob.setRotation(rotation);
}
