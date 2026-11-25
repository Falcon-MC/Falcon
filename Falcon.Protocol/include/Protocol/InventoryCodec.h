#pragma once

#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Protocol/PacketCodecContext.h"
#include "Protocol/Types/FullContainerName.h"
#include "Protocol/Types/InventoryActionData.h"
#include "Protocol/Types/InventorySource.h"
#include "Protocol/Types/ItemStackRequest.h"
#include "Protocol/Types/ItemStackRequestSlotData.h"
#include "Protocol/Types/ItemUseTransaction.h"
#include "Protocol/Types/PlayerBlockActionData.h"

class InventoryCodec {
public:
    static void writeFullContainerName(BinaryStream &stream, const FullContainerName &name);

    static FullContainerName readFullContainerName(ReadOnlyBinaryStream &stream);

    static void writeInventorySource(BinaryStream &stream, const PacketCodecContext &context,
                                     const InventorySource &source);

    static InventorySource readInventorySource(ReadOnlyBinaryStream &stream, const PacketCodecContext &context);

    static void writeInventoryActions(BinaryStream &stream, const PacketCodecContext &context,
                                       const std::vector<InventoryActionData> &actions);

    static void readInventoryActions(ReadOnlyBinaryStream &stream, const PacketCodecContext &context,
                                      std::vector<InventoryActionData> &actions);

    static void writeStackRequestSlotInfo(BinaryStream &stream, const ItemStackRequestSlotData &data);

    static ItemStackRequestSlotData readStackRequestSlotInfo(ReadOnlyBinaryStream &stream);

    static void writeItemStackRequest(BinaryStream &stream, const PacketCodecContext &context, const ItemStackRequest &request);

    static ItemStackRequest readItemStackRequest(ReadOnlyBinaryStream &stream, const PacketCodecContext &context);

    static void writeItemUseTransaction(BinaryStream &stream, const PacketCodecContext &context, const ItemUseTransaction &transaction);

    static ItemUseTransaction readItemUseTransaction(ReadOnlyBinaryStream &stream, const PacketCodecContext &context);

    static void writePlayerBlockActionData(BinaryStream &stream, const PlayerBlockActionData &data);

    static PlayerBlockActionData readPlayerBlockActionData(ReadOnlyBinaryStream &stream);
};
