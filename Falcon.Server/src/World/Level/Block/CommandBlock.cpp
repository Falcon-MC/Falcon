#include "World/Level/Block/CommandBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(CommandBlock, 30);

#include "World/Containers/Managers/Models/CommandBlockContainerManagerModel.h"

bool CommandBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:command_block" || identifier == "minecraft:repeating_command_block"
           || identifier == "minecraft:chain_command_block";
}

bool CommandBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                              const BlockState &state) const {
    (void) state;

    CommandBlockContainerManagerModel model;
    model.open(owner, player, position);
    return true;
}
