#pragma once

#include <string>

class BlockActor;
class ItemStack;
class PacketCodecContext;

namespace BlockPickItem {
    // not always the block itself (wall sign -> sign, lit furnace -> furnace, crop -> seeds);
    // empty for unpickable blocks: fire, portals, liquids
    std::string identifierFor(const std::string &blockIdentifier);

    // under BlockEntityTag with the "+(DATA)" lore; links to other blocks (chest pair) left out
    void attachBlockData(ItemStack &item, const BlockActor &actor);

    void restoreBlockData(BlockActor &actor, const ItemStack &item, const PacketCodecContext &context);
}
