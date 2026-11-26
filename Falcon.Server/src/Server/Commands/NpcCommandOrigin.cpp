#include "Server/Commands/NpcCommandOrigin.h"

#include "World/Actor/Misc/NpcActor.h"

std::string NpcCommandOrigin::getSenderName() const {
    return mNpc.getName();
}

Vector3f NpcCommandOrigin::getPosition() {
    return mNpc.getPosition();
}

Vector3f NpcCommandOrigin::getRotation() {
    return mNpc.getRotation();
}

Actor *NpcCommandOrigin::getEntity() {
    return &mNpc;
}
