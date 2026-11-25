#include "Protocol/Packets/CraftingDataPacket.h"

#include "Core/NBT/NbtIo.h"
#include "Protocol/NetworkPacketHandler.h"

CraftingDataPacket::CraftingDataPacket() = default;

namespace {
    const std::string EMPTY_ITEM_EXTRA_DATA(10, '\0');
    const std::string EMPTY_SHIELD_EXTRA_DATA(18, '\0');
    const std::string DESCRIPTOR_NAME = "name";
    const std::string DESCRIPTOR_MOLANG = "molang";
    const std::string DESCRIPTOR_ITEM_TAG = "item_tag";
    const int32_t WILDCARD_AUX_VALUE = 0x7fff;
    const uint16_t NBT_VERSIONED_MARKER = 0xffff;

    void writeIngredient(BinaryStream &stream, const RecipeIngredientEntry &ingredient) {
        if (!ingredient.mHasItem) {
            stream.putUnsignedVarInt(0);
            stream.putVarInt(WILDCARD_AUX_VALUE);
            stream.putVarInt(ingredient.mCount);
            return;
        }

        stream.putUnsignedVarInt(1);

        switch (ingredient.mType) {
            case RecipeIngredientType::Molang:
                stream.putString(DESCRIPTOR_MOLANG);
                stream.putString(ingredient.mMolangExpression);
                stream.putLShort((uint16_t) ingredient.mMolangVersion);
                break;
            case RecipeIngredientType::ItemTag:
                stream.putString(DESCRIPTOR_ITEM_TAG);
                stream.putString(ingredient.mItemTag);
                stream.putVarInt(WILDCARD_AUX_VALUE);
                break;
            default:
                stream.putString(DESCRIPTOR_NAME);
                stream.putString(ingredient.mItemId);
                stream.putVarInt(ingredient.mAuxValue);
                break;
        }

        stream.putVarInt(ingredient.mCount);
    }

    RecipeIngredientEntry readIngredient(ReadOnlyBinaryStream &stream) {
        RecipeIngredientEntry ingredient;

        if (stream.getUnsignedVarInt() == 0) {
            stream.getVarInt();
            ingredient.mCount = stream.getVarInt();
            return ingredient;
        }

        ingredient.mHasItem = true;

        const std::string type = stream.getString();
        if (type == DESCRIPTOR_NAME) {
            ingredient.mType = RecipeIngredientType::Name;
            ingredient.mItemId = stream.getString();
            ingredient.mAuxValue = stream.getVarInt();
        } else if (type == DESCRIPTOR_MOLANG) {
            ingredient.mType = RecipeIngredientType::Molang;
            ingredient.mMolangExpression = stream.getString();
            ingredient.mMolangVersion = stream.getSignedLShort();
        } else if (type == DESCRIPTOR_ITEM_TAG) {
            ingredient.mType = RecipeIngredientType::ItemTag;
            ingredient.mItemTag = stream.getString();
            stream.getVarInt();
        } else {
            throw BinaryDataException("Unknown item descriptor type \"" + type + "\"");
        }

        ingredient.mCount = stream.getVarInt();
        return ingredient;
    }

    void writeOutput(BinaryStream &stream, const PacketCodecContext &context, const RecipeOutputEntry &output) {
        stream.putVarInt(context.toNetworkItemId(output.mRuntimeId));
        stream.putLShort((uint16_t) output.mCount);
        stream.putUnsignedVarInt((uint32_t) output.mMeta);
        stream.putVarInt(context.toNetworkBlockId(output.mBlockRuntimeId));

        if (output.mTag.getType() == Tag::Type::End) {
            stream.putString(output.mIsShield ? EMPTY_SHIELD_EXTRA_DATA : EMPTY_ITEM_EXTRA_DATA);
            return;
        }

        BinaryStream extraData;
        extraData.putLShort(NBT_VERSIONED_MARKER);
        extraData.putByte(1);
        NbtIo::writeTag(extraData, output.mTag, NbtVariant::LittleEndian);
        extraData.putLInt(0);
        extraData.putLInt(0);
        if (output.mIsShield)
            extraData.putLLong(0);

        stream.putString(extraData.getBuffer());
    }

    RecipeOutputEntry readOutput(ReadOnlyBinaryStream &stream) {
        RecipeOutputEntry output;
        output.mRuntimeId = stream.getVarInt();
        output.mCount = stream.getLShort();
        output.mMeta = (int32_t) stream.getUnsignedVarInt();
        output.mBlockRuntimeId = stream.getVarInt();

        const std::string extraDataBytes = stream.getString();
        ReadOnlyBinaryStream extraData(extraDataBytes);

        const uint16_t nbtSize = extraData.getLShort();
        if (nbtSize == NBT_VERSIONED_MARKER) {
            const unsigned char version = extraData.getByte();
            if (version != 1)
                throw BinaryDataException("Unexpected item extra data NBT version " + std::to_string((int) version));
            output.mTag = NbtIo::readTag(extraData, NbtVariant::LittleEndian);
        } else if (nbtSize > 0) {
            output.mTag = NbtIo::readTag(extraData, NbtVariant::LittleEndian);
        }

        return output;
    }

    void writeUnlockingRequirement(BinaryStream &stream, const CraftingRecipeEntry &recipe) {
        stream.putBool(recipe.mHasUnlockingRequirement);
        if (!recipe.mHasUnlockingRequirement)
            return;

        stream.putVarInt(recipe.mUnlockingContext);
        stream.putBool(recipe.mHasUnlockingIngredients);
        if (!recipe.mHasUnlockingIngredients)
            return;

        stream.putArrayLength((uint32_t) recipe.mUnlockingIngredients.size());
        for (const RecipeIngredientEntry &ingredient: recipe.mUnlockingIngredients)
            writeIngredient(stream, ingredient);
    }

    void readUnlockingRequirement(ReadOnlyBinaryStream &stream, CraftingRecipeEntry &recipe) {
        recipe.mHasUnlockingRequirement = stream.getBool();
        recipe.mHasUnlockingIngredients = false;
        recipe.mUnlockingIngredients.clear();
        if (!recipe.mHasUnlockingRequirement)
            return;

        recipe.mUnlockingContext = stream.getVarInt();
        recipe.mHasUnlockingIngredients = stream.getBool();
        if (!recipe.mHasUnlockingIngredients)
            return;

        const uint32_t count = stream.getArrayLength();
        for (uint32_t i = 0; i < count; i++)
            recipe.mUnlockingIngredients.push_back(readIngredient(stream));
    }

    void writeRecipe(BinaryStream &stream, const PacketCodecContext &context, const CraftingRecipeEntry &recipe,
                     bool shaped) {
        stream.putString(recipe.mRecipeId);

        if (shaped) {
            stream.putVarInt(recipe.mWidth);
            stream.putVarInt(recipe.mHeight);
        }

        stream.putUnsignedVarInt((uint32_t) recipe.mInputs.size());
        for (const RecipeIngredientEntry &ingredient: recipe.mInputs)
            writeIngredient(stream, ingredient);

        stream.putUnsignedVarInt((uint32_t) recipe.mOutputs.size());
        for (const RecipeOutputEntry &output: recipe.mOutputs)
            writeOutput(stream, context, output);

        stream.putUuid(recipe.mUuid);
        stream.putString(recipe.mBlockName);
        stream.putVarInt(recipe.mPriority);

        if (shaped)
            stream.putBool(recipe.mSymmetric);

        writeUnlockingRequirement(stream, recipe);
        stream.putUnsignedVarInt((uint32_t) recipe.mRecipeNetId);
    }

    CraftingRecipeEntry readRecipe(ReadOnlyBinaryStream &stream, bool shaped) {
        CraftingRecipeEntry recipe;
        recipe.mRecipeId = stream.getString();

        if (shaped) {
            recipe.mWidth = stream.getVarInt();
            recipe.mHeight = stream.getVarInt();
        }

        const uint32_t inputCount = stream.getArrayLength();
        for (uint32_t i = 0; i < inputCount; i++)
            recipe.mInputs.push_back(readIngredient(stream));

        const uint32_t outputCount = stream.getArrayLength();
        for (uint32_t i = 0; i < outputCount; i++)
            recipe.mOutputs.push_back(readOutput(stream));

        recipe.mUuid = stream.getUuid();
        recipe.mBlockName = stream.getString();
        recipe.mPriority = stream.getVarInt();

        if (shaped)
            recipe.mSymmetric = stream.getBool();

        readUnlockingRequirement(stream, recipe);
        recipe.mRecipeNetId = (int32_t) stream.getUnsignedVarInt();
        return recipe;
    }

    void writeRecipes(BinaryStream &stream, const PacketCodecContext &context,
                      const std::vector<CraftingRecipeEntry> &recipes, bool shaped) {
        stream.putUnsignedVarInt((uint32_t) recipes.size());
        for (const CraftingRecipeEntry &recipe: recipes)
            writeRecipe(stream, context, recipe, shaped);
    }

    std::vector<CraftingRecipeEntry> readRecipes(ReadOnlyBinaryStream &stream, bool shaped) {
        std::vector<CraftingRecipeEntry> recipes;
        const uint32_t count = stream.getArrayLength();
        recipes.reserve(count);
        for (uint32_t i = 0; i < count; i++)
            recipes.push_back(readRecipe(stream, shaped));
        return recipes;
    }

    void writeSmithingRecipes(BinaryStream &stream, const PacketCodecContext &context,
                              const std::vector<SmithingRecipeEntry> &recipes, bool transform) {
        stream.putUnsignedVarInt((uint32_t) recipes.size());
        for (const SmithingRecipeEntry &recipe: recipes) {
            stream.putString(recipe.mRecipeId);
            writeIngredient(stream, recipe.mTemplate);
            writeIngredient(stream, recipe.mInput);
            writeIngredient(stream, recipe.mAddition);
            if (transform)
                writeOutput(stream, context, recipe.mOutput);
            stream.putString(recipe.mBlockName);
            stream.putUnsignedVarInt((uint32_t) recipe.mRecipeNetId);
        }
    }

    std::vector<SmithingRecipeEntry> readSmithingRecipes(ReadOnlyBinaryStream &stream, bool transform) {
        std::vector<SmithingRecipeEntry> recipes;
        const uint32_t count = stream.getArrayLength();
        recipes.reserve(count);
        for (uint32_t i = 0; i < count; i++) {
            SmithingRecipeEntry recipe;
            recipe.mRecipeId = stream.getString();
            recipe.mTemplate = readIngredient(stream);
            recipe.mInput = readIngredient(stream);
            recipe.mAddition = readIngredient(stream);
            if (transform)
                recipe.mOutput = readOutput(stream);
            recipe.mBlockName = stream.getString();
            recipe.mRecipeNetId = (int32_t) stream.getUnsignedVarInt();
            recipes.push_back(std::move(recipe));
        }
        return recipes;
    }
}

void CraftingDataPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    writeRecipes(stream, context, mShapedRecipes, true);
    writeRecipes(stream, context, mShapelessRecipes, false);

    stream.putUnsignedVarInt((uint32_t) mMultiRecipes.size());
    for (const MultiRecipeEntry &recipe: mMultiRecipes) {
        stream.putUuid(recipe.mUuid);
        stream.putUnsignedVarInt((uint32_t) recipe.mRecipeNetId);
    }

    writeRecipes(stream, context, mUserDataShapelessRecipes, false);
    writeRecipes(stream, context, mShapelessChemistryRecipes, false);
    writeRecipes(stream, context, mShapedChemistryRecipes, true);
    writeSmithingRecipes(stream, context, mSmithingTransformRecipes, true);
    writeSmithingRecipes(stream, context, mSmithingTrimRecipes, false);

    stream.putArrayLength((uint32_t) mPotionMixes.size());
    for (const PotionMixEntry &mix: mPotionMixes) {
        stream.putVarInt(context.toNetworkItemId(mix.mInputId));
        stream.putVarInt(mix.mInputMeta);
        stream.putVarInt(context.toNetworkItemId(mix.mReagentId));
        stream.putVarInt(mix.mReagentMeta);
        stream.putVarInt(context.toNetworkItemId(mix.mOutputId));
        stream.putVarInt(mix.mOutputMeta);
    }

    stream.putArrayLength((uint32_t) mPotionContainerMixes.size());
    for (const PotionContainerMixEntry &mix: mPotionContainerMixes) {
        stream.putVarInt(context.toNetworkItemId(mix.mInputId));
        stream.putVarInt(context.toNetworkItemId(mix.mReagentId));
        stream.putVarInt(context.toNetworkItemId(mix.mOutputId));
    }

    stream.putArrayLength((uint32_t) mMaterialReducers.size());
    for (const MaterialReducerEntry &reducer: mMaterialReducers) {
        stream.putVarInt((context.toNetworkItemId(reducer.mInputId) << 16) | (reducer.mInputMeta & 0x7fff));
        stream.putArrayLength((uint32_t) reducer.mOutputs.size());
        for (const MaterialReducerOutputEntry &output: reducer.mOutputs) {
            stream.putVarInt(context.toNetworkItemId(output.mItemId));
            stream.putVarInt(output.mCount);
        }
    }

    stream.putBool(mCleanRecipes);
}

void CraftingDataPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mShapedRecipes = readRecipes(stream, true);
    mShapelessRecipes = readRecipes(stream, false);

    mMultiRecipes.clear();
    const uint32_t multiCount = stream.getArrayLength();
    for (uint32_t i = 0; i < multiCount; i++) {
        MultiRecipeEntry recipe;
        recipe.mUuid = stream.getUuid();
        recipe.mRecipeNetId = (int32_t) stream.getUnsignedVarInt();
        mMultiRecipes.push_back(recipe);
    }

    mUserDataShapelessRecipes = readRecipes(stream, false);
    mShapelessChemistryRecipes = readRecipes(stream, false);
    mShapedChemistryRecipes = readRecipes(stream, true);
    mSmithingTransformRecipes = readSmithingRecipes(stream, true);
    mSmithingTrimRecipes = readSmithingRecipes(stream, false);

    mPotionMixes.clear();
    const uint32_t potionCount = stream.getArrayLength();
    for (uint32_t i = 0; i < potionCount; i++) {
        PotionMixEntry mix;
        mix.mInputId = stream.getVarInt();
        mix.mInputMeta = stream.getVarInt();
        mix.mReagentId = stream.getVarInt();
        mix.mReagentMeta = stream.getVarInt();
        mix.mOutputId = stream.getVarInt();
        mix.mOutputMeta = stream.getVarInt();
        mPotionMixes.push_back(mix);
    }

    mPotionContainerMixes.clear();
    const uint32_t containerCount = stream.getArrayLength();
    for (uint32_t i = 0; i < containerCount; i++) {
        PotionContainerMixEntry mix;
        mix.mInputId = stream.getVarInt();
        mix.mReagentId = stream.getVarInt();
        mix.mOutputId = stream.getVarInt();
        mPotionContainerMixes.push_back(mix);
    }

    mMaterialReducers.clear();
    const uint32_t reducerCount = stream.getArrayLength();
    for (uint32_t i = 0; i < reducerCount; i++) {
        MaterialReducerEntry reducer;
        const int32_t inputIdAndData = stream.getVarInt();
        reducer.mInputId = inputIdAndData >> 16;
        reducer.mInputMeta = inputIdAndData & 0x7fff;
        const uint32_t outputCount = stream.getArrayLength();
        for (uint32_t j = 0; j < outputCount; j++) {
            MaterialReducerOutputEntry output;
            output.mItemId = stream.getVarInt();
            output.mCount = stream.getVarInt();
            reducer.mOutputs.push_back(output);
        }
        mMaterialReducers.push_back(std::move(reducer));
    }

    mCleanRecipes = stream.getBool();
}

void CraftingDataPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
