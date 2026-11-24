#include "World/Level/Block/Actor/DataDrivenBlockActor.h"

#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Level/Level.h"

// the common fields, the dynamic properties and the entity storage are all its data
Tag DataDrivenBlockActor::saveNbt() const {
    return Tag::ofCompound();
}

void DataDrivenBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) data;
    (void) context;
}

bool DataDrivenBlockActor::tick(ServerNetworkHandler &owner) {
    (void) owner;
    if (mLevel == nullptr)
        return true;
    const BlockState *state = mLevel->peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    if (state == nullptr)
        return true;
    const CustomBlockDefinition *custom = CustomContentRegistry::getInstance().findBlock(state->mName);
    return custom != nullptr && custom->mEntityStorage;
}
