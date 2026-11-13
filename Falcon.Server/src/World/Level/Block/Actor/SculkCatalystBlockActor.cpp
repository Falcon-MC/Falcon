#include "World/Level/Block/Actor/SculkCatalystBlockActor.h"

#include "World/Level/Block/Actor/BlockActorClassRegistry.h"

FALCON_REGISTER_BLOCK_ACTOR(SculkCatalystBlockActor);

Tag SculkCatalystBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    mSpreader.save(data);
    return data;
}

void SculkCatalystBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;
    mSpreader.load(data);
}
