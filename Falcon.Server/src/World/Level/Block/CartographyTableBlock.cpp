#include "World/Level/Block/CartographyTableBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(CartographyTableBlock, 10);

#include "World/Containers/Managers/Models/CartographyContainerManagerModel.h"
#include "World/Level/Block/BlockTypeIds.h"

CartographyTableBlock::CartographyTableBlock()
        : Block(BlockTypeIds::CARTOGRAPHY_TABLE, IDENTIFIER, "Cartography Table") {}

CartographyTableBlock::CartographyTableBlock(const Block &block) : Block(block) {}

bool CartographyTableBlock::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

bool CartographyTableBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                       const BlockState &state) const {
    (void) state;

    CartographyContainerManagerModel model;
    model.open(owner, player, position);
    return true;
}
