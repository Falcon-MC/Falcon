#pragma once

#include <string>

class BlockActor;
class ItemStack;
class PacketCodecContext;

namespace BlockPickItem {
    /**
     * The item a player gets when picking the given block, which is not always the block itself: a wall sign
     * gives its sign, a lit furnace gives a furnace, a crop gives its seeds. Returns an empty string for
     * blocks that cannot be picked, such as fire, portals or liquids.
     */
    std::string identifierFor(const std::string &blockIdentifier);

    /**
     * Copies the block actor's data (a chest's items, a sign's text...) into the picked item under
     * BlockEntityTag and marks it with the "+(DATA)" lore. Links to other blocks, such as a chest pair, are left out.
     */
    void attachBlockData(ItemStack &item, const BlockActor &actor);

    /** Applies the BlockEntityTag carried by the item, if any, to the block actor it was just placed as. */
    void restoreBlockData(BlockActor &actor, const ItemStack &item, const PacketCodecContext &context);
}
