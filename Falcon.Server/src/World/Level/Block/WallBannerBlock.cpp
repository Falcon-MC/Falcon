#include "World/Level/Block/WallBannerBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/Actor/BannerBlockActor.h"

FALCON_REGISTER_BLOCK(WallBannerBlock, 185);

bool WallBannerBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:wall_banner";
}

void WallBannerBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                               const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) blockFace;
    BannerBlockActor::placeFromItem(owner, player, position, state, usedItem);
}
