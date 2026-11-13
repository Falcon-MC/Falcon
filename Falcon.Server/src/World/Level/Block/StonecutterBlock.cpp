#include "World/Level/Block/StonecutterBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(StonecutterBlock, 10);

#include "World/Containers/Managers/Models/StonecutterContainerManagerModel.h"
#include "World/Level/Block/BlockTypeIds.h"

StonecutterBlock::StonecutterBlock() : Block(BlockTypeIds::STONECUTTER_BLOCK, IDENTIFIER, "Stonecutter") {}

StonecutterBlock::StonecutterBlock(const Block &block) : Block(block) {}

bool StonecutterBlock::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

bool StonecutterBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                  const BlockState &state) const {
    (void) state;

    StonecutterContainerManagerModel model;
    model.open(owner, player, position);
    return true;
}
