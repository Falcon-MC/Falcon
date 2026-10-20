#include "World/Level/Block/Actor/BlockActor.h"

#include "World/Level/Storage/FalconDataVersion.h"

Tag BlockActor::getSpawnCompound() const {
    Tag data = Tag::ofCompound();
    data.putString("id", getBlockActorId());
    data.putInt("x", mPosition.x);
    data.putInt("y", mPosition.y);
    data.putInt("z", mPosition.z);
    return data;
}

Tag BlockActor::saveWithPosition() const {
    Tag data = saveNbt();
    data.putString("id", getBlockActorId());
    data.putLong(FalconDataVersion::TAG, FalconDataVersion::CURRENT);
    data.putInt("x", mPosition.x);
    data.putInt("y", mPosition.y);
    data.putInt("z", mPosition.z);
    return data;
}
