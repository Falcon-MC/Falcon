#include "Item/CraftingRecipeTable.h"
#include "CraftingRecipeJson.h"
#include "Core/Json/Json.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
    struct IngredientChoice {
        bool mEmpty = true;
        std::string mItemId;
        int32_t mAuxValue = 0;
        int32_t mCount = 0;
    };

    struct RecipeStorage {
        std::vector<CraftingIngredientData> mIngredients;
        std::vector<CraftingOutputData> mOutputs;
        std::vector<CraftingRecipeData> mRecipes;
        std::vector<FurnaceRecipeData> mFurnaceRecipes;
        std::deque<std::string> mStrings;
        bool mLoaded = false;

        const char *store(const std::string &value) {
            mStrings.push_back(value);
            return mStrings.back().c_str();
        }
    };

    using TagMap = std::unordered_map<std::string, std::vector<std::string>>;

    TagMap loadTags() {
        TagMap tags;
        std::unique_ptr<JsonValue> root = JsonParser(FalconCraftingData::kItemTagsJson).parse();
        if (root == nullptr || root->mType != JsonValue::Type::Object)
            return tags;

        for (const auto &entry: root->mObject) {
            if (entry.second->mType != JsonValue::Type::Array)
                continue;
            std::vector<std::string> values;
            for (const std::unique_ptr<JsonValue> &value: entry.second->mArray) {
                if (value->mType == JsonValue::Type::String)
                    values.push_back(value->mString);
            }
            tags.emplace(entry.first, std::move(values));
        }
        return tags;
    }

    std::vector<IngredientChoice> parseIngredient(const JsonValue &value, const TagMap &tags) {
        const JsonValue *typeValue = value.get("type");
        const std::string type = typeValue == nullptr ? std::string() : typeValue->string();
        const JsonValue *countValue = value.get("count");
        const int32_t count = countValue == nullptr ? 1 : countValue->integer(1);
        if (count <= 0)
            return {};

        if (type == "item_tag") {
            const JsonValue *tag = value.get("itemTag");
            if (tag == nullptr)
                return {};
            const auto it = tags.find(tag->string());
            if (it == tags.end())
                return {};
            std::vector<IngredientChoice> choices;
            for (const std::string &itemId: it->second)
                choices.push_back({false, itemId, -1, count});
            return choices;
        }

        if (type != "name" && type != "default")
            return {};
        const JsonValue *itemId = value.get("itemId");
        if (itemId == nullptr || itemId->mType != JsonValue::Type::String)
            return {};
        const JsonValue *auxValue = value.get("auxValue");
        const int32_t aux = auxValue == nullptr ? 0 : auxValue->integer(0);
        return {{false, itemId->mString, aux, count}};
    }

    void addRecipe(RecipeStorage &storage, const JsonValue &source, int32_t width, int32_t height,
                   const std::vector<IngredientChoice> &ingredients, size_t variant) {
        const JsonValue *outputData = source.get("output");
        if (outputData == nullptr || outputData->mType != JsonValue::Type::Array || outputData->mArray.empty())
            return;

        const size_t outputOffset = storage.mOutputs.size();
        for (const std::unique_ptr<JsonValue> &output: outputData->mArray) {
            const JsonValue *itemId = output->get("id");
            if (itemId == nullptr || itemId->mType != JsonValue::Type::String)
                continue;
            const JsonValue *countValue = output->get("count");
            const int32_t count = countValue == nullptr ? 1 : countValue->integer(1);
            storage.mOutputs.push_back({storage.store(itemId->mString), count});
        }
        if (storage.mOutputs.size() == outputOffset)
            return;

        const size_t ingredientOffset = storage.mIngredients.size();
        for (const IngredientChoice &ingredient: ingredients) {
            if (ingredient.mEmpty)
                storage.mIngredients.push_back({nullptr, 0, 0});
            else
                storage.mIngredients.push_back({storage.store(ingredient.mItemId), ingredient.mAuxValue,
                                                ingredient.mCount});
        }

        const JsonValue *id = source.get("id");
        const JsonValue *uuid = source.get("uuid");
        const std::string baseId = id == nullptr ? "falcon.recipe" : id->string("falcon.recipe");
        const std::string recipeId = variant == 0 ? baseId : baseId + "#" + std::to_string(variant);
        const std::string uuidValue = uuid == nullptr ? std::string() : uuid->string();
        const int32_t priority = source.get("priority") == nullptr ? 0 : source.get("priority")->integer(0);
        storage.mRecipes.push_back({storage.store(recipeId), width, height, (uint32_t) ingredientOffset,
                                    (uint32_t) ingredients.size(), (uint32_t) outputOffset,
                                    (uint32_t) (storage.mOutputs.size() - outputOffset), storage.store(uuidValue),
                                    priority});
    }

    void addVariants(RecipeStorage &storage, const JsonValue &source, int32_t width, int32_t height,
                     const std::vector<std::vector<IngredientChoice>> &choices) {
        std::vector<IngredientChoice> selected(choices.size());
        size_t variant = 0;
        std::function<void(size_t)> visit = [&](size_t index) {
            if (index == choices.size()) {
                addRecipe(storage, source, width, height, selected, variant++);
                return;
            }
            for (const IngredientChoice &choice: choices[index]) {
                selected[index] = choice;
                visit(index + 1);
            }
        };
        visit(0);
    }

    void loadShapedRecipe(RecipeStorage &storage, const JsonValue &source, const TagMap &tags) {
        const JsonValue *shapeData = source.get("shape");
        const JsonValue *inputData = source.get("input");
        if (shapeData == nullptr || inputData == nullptr || shapeData->mType != JsonValue::Type::Array ||
            inputData->mType != JsonValue::Type::Object || shapeData->mArray.empty())
            return;

        std::vector<std::string> shape;
        for (const std::unique_ptr<JsonValue> &row: shapeData->mArray) {
            if (row->mType != JsonValue::Type::String)
                return;
            shape.push_back(row->mString);
        }
        const int32_t height = (int32_t) shape.size();
        const int32_t width = (int32_t) shape.front().size();
        if (width <= 0 || width > 3 || height <= 0 || height > 3)
            return;

        std::unordered_map<char, size_t> symbolIndexes;
        std::vector<std::vector<IngredientChoice>> symbolChoices;
        for (const std::string &row: shape) {
            if ((int32_t) row.size() != width)
                return;
            for (char symbol: row) {
                if (symbol == ' ' || symbolIndexes.count(symbol) != 0)
                    continue;
                const std::string key(1, symbol);
                const JsonValue *ingredient = inputData->get(key);
                if (ingredient == nullptr)
                    return;
                std::vector<IngredientChoice> choices = parseIngredient(*ingredient, tags);
                if (choices.empty())
                    return;
                symbolIndexes[symbol] = symbolChoices.size();
                symbolChoices.push_back(std::move(choices));
            }
        }

        std::vector<IngredientChoice> selected(symbolChoices.size());
        std::vector<std::vector<IngredientChoice>> variants;
        std::function<void(size_t)> visit = [&](size_t index) {
            if (index == symbolChoices.size()) {
                std::vector<IngredientChoice> slots;
                slots.reserve((size_t) width * (size_t) height);
                for (const std::string &row: shape) {
                    for (char symbol: row) {
                        if (symbol == ' ')
                            slots.push_back({});
                        else
                            slots.push_back(selected[symbolIndexes[symbol]]);
                    }
                }
                variants.push_back(std::move(slots));
                return;
            }
            for (const IngredientChoice &choice: symbolChoices[index]) {
                selected[index] = choice;
                visit(index + 1);
            }
        };
        visit(0);

        size_t variant = 0;
        for (const std::vector<IngredientChoice> &ingredients: variants)
            addRecipe(storage, source, width, height, ingredients, variant++);
    }

    void loadShapelessRecipe(RecipeStorage &storage, const JsonValue &source, const TagMap &tags) {
        const JsonValue *inputData = source.get("input");
        if (inputData == nullptr || inputData->mType != JsonValue::Type::Array)
            return;

        std::vector<std::vector<IngredientChoice>> choices;
        for (const std::unique_ptr<JsonValue> &input: inputData->mArray) {
            std::vector<IngredientChoice> parsed = parseIngredient(*input, tags);
            if (parsed.empty())
                return;
            choices.push_back(std::move(parsed));
        }
        addVariants(storage, source, 0, 0, choices);
    }

    void loadFurnaceRecipe(RecipeStorage &storage, const JsonValue &source, const TagMap &tags) {
        const JsonValue *inputData = source.get("input");
        const JsonValue *outputData = source.get("output");
        if (inputData == nullptr || inputData->mType != JsonValue::Type::Array || inputData->mArray.empty()
            || outputData == nullptr || outputData->mType != JsonValue::Type::Array || outputData->mArray.empty()) {
            return;
        }

        const std::vector<IngredientChoice> choices = parseIngredient(*inputData->mArray.front(), tags);
        if (choices.empty()) {
            return;
        }

        const JsonValue *output = outputData->mArray.front()->get("id");
        if (output == nullptr || output->mType != JsonValue::Type::String) {
            return;
        }

        const JsonValue *countValue = outputData->mArray.front()->get("count");
        const int32_t outputCount = countValue == nullptr ? 1 : countValue->integer(1);
        if (outputCount <= 0) {
            return;
        }

        const JsonValue *id = source.get("id");
        const std::string baseId = id == nullptr ? "falcon.furnace" : id->string("falcon.furnace");
        const int32_t priority = source.get("priority") == nullptr ? 0 : source.get("priority")->integer(0);
        for (size_t index = 0; index < choices.size(); ++index) {
            const IngredientChoice &choice = choices[index];
            if (choice.mEmpty || choice.mItemId.empty() || choice.mCount <= 0) {
                continue;
            }
            const std::string recipeId = index == 0 ? baseId : baseId + "#" + std::to_string(index);
            storage.mFurnaceRecipes.push_back({storage.store(recipeId), storage.store(choice.mItemId),
                                               choice.mAuxValue, choice.mCount, storage.store(output->mString),
                                               outputCount, priority});
        }
    }

    RecipeStorage &getStorage() {
        static RecipeStorage storage;
        if (storage.mLoaded)
            return storage;
        storage.mLoaded = true;

        std::unique_ptr<JsonValue> root = JsonParser(FalconCraftingData::kRecipesJson).parse();
        if (root == nullptr)
            return storage;
        const JsonValue *recipes = root->get("recipes");
        if (recipes == nullptr || recipes->mType != JsonValue::Type::Array)
            return storage;

        const TagMap tags = loadTags();
        for (const std::unique_ptr<JsonValue> &recipe: recipes->mArray) {
            const JsonValue *type = recipe->get("type");
            const JsonValue *block = recipe->get("block");
            if (type == nullptr || block == nullptr)
                continue;

            if (block->string() == "furnace") {
                if (type->integer(-1) == 1) {
                    loadFurnaceRecipe(storage, *recipe, tags);
                }
                continue;
            }

            if (block->string() != "crafting_table")
                continue;

            switch (type->integer(-1)) {
                case 0:
                    loadShapedRecipe(storage, *recipe, tags);
                    break;
                case 1:
                case 3:
                    loadShapelessRecipe(storage, *recipe, tags);
                    break;
                default:
                    break;
            }
        }
        return storage;
    }
}

const CraftingRecipeData *CraftingRecipeTable::getRecipes() {
    return getStorage().mRecipes.data();
}

size_t CraftingRecipeTable::getRecipeCount() {
    return getStorage().mRecipes.size();
}

const CraftingIngredientData *CraftingRecipeTable::getIngredients() {
    return getStorage().mIngredients.data();
}

const CraftingOutputData *CraftingRecipeTable::getOutputs() {
    return getStorage().mOutputs.data();
}

const FurnaceRecipeData *CraftingRecipeTable::getFurnaceRecipes() {
    return getStorage().mFurnaceRecipes.data();
}

size_t CraftingRecipeTable::getFurnaceRecipeCount() {
    return getStorage().mFurnaceRecipes.size();
}
