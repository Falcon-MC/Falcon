#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/CraftingRecipeEntry.h"

#include <vector>

class CraftingDataPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::CraftingData;

    CraftingDataPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "CraftingDataPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<CraftingRecipeEntry> mShapedRecipes;
    std::vector<CraftingRecipeEntry> mShapelessRecipes;
    std::vector<MultiRecipeEntry> mMultiRecipes;
    std::vector<CraftingRecipeEntry> mUserDataShapelessRecipes;
    std::vector<CraftingRecipeEntry> mShapelessChemistryRecipes;
    std::vector<CraftingRecipeEntry> mShapedChemistryRecipes;
    std::vector<SmithingRecipeEntry> mSmithingTransformRecipes;
    std::vector<SmithingRecipeEntry> mSmithingTrimRecipes;
    std::vector<PotionMixEntry> mPotionMixes;
    std::vector<PotionContainerMixEntry> mPotionContainerMixes;
    std::vector<MaterialReducerEntry> mMaterialReducers;
    bool mCleanRecipes = false;
};
