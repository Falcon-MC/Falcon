#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/InventoryActionData.h"
#include "Protocol/Types/InventoryTransactionType.h"
#include "Protocol/Types/ItemStack.h"
#include "Protocol/Types/ItemUseTransaction.h"
#include "Protocol/Types/LegacySetItemSlotData.h"

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "Protocol/Types/HandSlot.h"

class InventoryTransactionPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::InventoryTransaction;

    InventoryTransactionPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "InventoryTransactionPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int32_t mLegacyRequestId = 0;
    std::vector<LegacySetItemSlotData> mLegacySlots;
    std::vector<InventoryActionData> mActions;
    InventoryTransactionType mTransactionType = InventoryTransactionType::Normal;
    int32_t mActionType = 0;
    int64_t mRuntimeActorId = 0;
    Vector3i mBlockPosition;
    int32_t mBlockFace = 0;
    int32_t mHotbarSlot = 0;
    HandSlot mHand = HandSlot::MainHand;
    ItemStack mItemInHand;
    Vector3f mPlayerPosition;
    Vector3f mClickPosition;
    Vector3f mHeadPosition;
    std::shared_ptr<BlockDefinition> mBlockDefinition;
    ItemUseTriggerType mTriggerType = ItemUseTriggerType::Unknown;
    ItemUsePredictedResult mClientInteractPrediction = ItemUsePredictedResult::Failure;
    int32_t mClientCooldownState = 0;
};
