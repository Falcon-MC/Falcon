#include "World/Level/Block/Actor/FlowerPotBlockActor.h"

namespace {
    const char *TAG_PLANT = "PlantBlock";
}

void FlowerPotBlockActor::_putFields(Tag &data) const {
    if (mHasPlant)
        data.put(TAG_PLANT, mPlant.toNbt());
}

Tag FlowerPotBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    _putFields(data);
    return data;
}

Tag FlowerPotBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    _putFields(data);
    return data;
}

void FlowerPotBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;

    const Tag *plant = data.get(TAG_PLANT);
    mHasPlant = plant != nullptr && plant->isCompound();
    if (!mHasPlant)
        return;

    const Tag *states = plant->get("states");
    mPlant = BlockState(plant->getString("name"), states != nullptr ? *states : Tag::ofCompound());
}
