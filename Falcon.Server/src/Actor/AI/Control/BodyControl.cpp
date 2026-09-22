#include "Actor/AI/Control/BodyControl.h"

#include "Actor/AI/Control/LookControl.h"
#include "Actor/AI/Control/MoveControl.h"
#include "Actor/Mob/MobActor.h"

void BodyControl::tick(MobActor &mob, const MoveControl &moveControl, const LookControl &lookControl) {
    if (!moveControl.hasWanted())
        return;

    Vector3f rotation = mob.getRotation();
    rotation.y = LookControl::yawTowards(mob.getPosition(), moveControl.getWantedPosition());
    if (!lookControl.hasLookAt())
        rotation.z = rotation.y;
    mob.setRotation(rotation);
}
