#pragma once

#include "Core/NBT/Tag.h"
#include "Protocol/PacketCodecContext.h"
#include "Protocol/Types/ItemStack.h"

class ItemStackNbt {
public:
    static Tag write(const ItemStack &item, int slot);

    static Tag write(const ItemStack &item);

    static ItemStack read(const Tag &data, const PacketCodecContext &context);

    static int readSlot(const Tag &data);
};
