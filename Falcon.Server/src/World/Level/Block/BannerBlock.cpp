#include "World/Level/Block/BannerBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/Actor/BannerBlockActor.h"

FALCON_REGISTER_BLOCK(BannerBlock, 200);

bool BannerBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:standing_banner";
}

void BannerBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                           const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) blockFace;
    BannerBlockActor::placeFromItem(owner, player, position, state, usedItem);
}

const char *BannerBlock::getBlockActorId() const {
    return BannerBlockActor::BLOCK_ACTOR_ID;
}
