#pragma once

#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Protocol/PacketCodecContext.h"
#include "Protocol/Types/ItemStack.h"

class ItemCodec {
public:
    static void writeItemInstance(BinaryStream &stream, const PacketCodecContext &context, const ItemStack &item);

    static ItemStack readItemInstance(ReadOnlyBinaryStream &stream, const PacketCodecContext &context);

    static void writeNetworkItemStackDescriptor(BinaryStream &stream, const PacketCodecContext &context, const ItemStack &item);

    static ItemStack readNetworkItemStackDescriptor(ReadOnlyBinaryStream &stream, const PacketCodecContext &context);

    static void writeRequestItemDescriptor(BinaryStream &stream, const PacketCodecContext &context, const ItemStack &item);

    static ItemStack readRequestItemDescriptor(ReadOnlyBinaryStream &stream, const PacketCodecContext &context);
};
