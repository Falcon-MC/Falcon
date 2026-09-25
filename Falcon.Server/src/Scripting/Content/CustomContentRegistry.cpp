#include "Scripting/Content/CustomContentRegistry.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Item/Item.h"
#include "Item/StringToItemParser.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Types/BlockDefinition.h"
#include "Protocol/Types/BlockDefinitionRegistry.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Protocol/Types/ItemDefinitionRegistry.h"
#include "Scripting/BehaviorPackManager.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

namespace {
    std::string readFile(const std::filesystem::path &path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return std::string();

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::string qualify(const std::string &identifier) {
        if (identifier.find(':') != std::string::npos)
            return identifier;
        return "minecraft:" + identifier;
    }

    std::string prettify(const std::string &identifier) {
        const size_t colon = identifier.find(':');
        std::string base = colon == std::string::npos ? identifier : identifier.substr(colon + 1);

        std::string result;
        bool capitalize = true;
        for (char character: base) {
            if (character == '_') {
                result.push_back(' ');
                capitalize = true;
                continue;
            }
            if (capitalize) {
                result.push_back((char) std::toupper((unsigned char) character));
                capitalize = false;
            } else {
                result.push_back(character);
            }
        }
        return result;
    }

    void readVec3(const json::Value *array, float *out) {
        if (array == nullptr || !array->isArray())
            return;
        for (size_t i = 0; i < 3 && i < array->mArray.size(); ++i)
            out[i] = (float) array->mArray[i]->number(out[i]);
    }

    void parseBox(const json::Value *value, CustomBlockBox &box) {
        if (value == nullptr)
            return;

        if (value->mType == json::Value::Type::Boolean) {
            box.mEnabled = value->boolean(true);
            return;
        }

        if (!value->isObject())
            return;

        box.mEnabled = true;
        readVec3(value->get("origin"), box.mOrigin);
        readVec3(value->get("size"), box.mSize);
    }

}

CustomContentRegistry &CustomContentRegistry::getInstance() {
    static CustomContentRegistry instance;
    return instance;
}

bool CustomContentRegistry::isCustomBlock(const std::string &identifier) const {
    for (const CustomBlockDefinition &block: mBlocks) {
        if (block.mIdentifier == identifier)
            return true;
    }
    return false;
}

const ActorPropertyDescription *CustomActorDefinition::findProperty(const std::string &name) const {
    for (const ActorPropertyDescription &property: mProperties) {
        if (property.mName == name)
            return &property;
    }
    return nullptr;
}

int32_t CustomContentRegistry::getItemMaxStackSize(const std::string &identifier) const {
    for (const CustomItemDefinition &item: mItems) {
        if (item.mIdentifier == identifier)
            return item.mMaxStackSize;
    }
    for (const CustomBlockDefinition &block: mBlocks) {
        if (block.mIdentifier == identifier)
            return 64;
    }
    return 0;
}

int32_t CustomContentRegistry::getDiggerSpeed(const std::string &itemId, const std::string &blockName,
                                              bool blockNeedsStandardTool, bool &useEfficiency) const {
    for (const CustomItemDefinition &item: mItems) {
        if (item.mIdentifier != itemId)
            continue;

        if (!item.mHasDigger)
            return 0;

        useEfficiency = item.mDiggerUseEfficiency;

        const auto it = item.mDiggerBlockSpeeds.find(blockName);
        if (it != item.mDiggerBlockSpeeds.end())
            return it->second;

        if (blockNeedsStandardTool)
            return item.mDiggerToolSpeed;

        return 0;
    }
    return 0;
}

const CustomActorDefinition *CustomContentRegistry::getActorDefinition(const std::string &identifier) const {
    for (const CustomActorDefinition &actor: mActors) {
        if (actor.mIdentifier == identifier)
            return &actor;
    }
    return nullptr;
}

bool CustomContentRegistry::hasIdentifier(const std::string &identifier) const {
    for (const CustomItemDefinition &item: mItems) {
        if (item.mIdentifier == identifier)
            return true;
    }
    return isCustomBlock(identifier) || getActorDefinition(identifier) != nullptr;
}

const CustomItemDefinition *CustomContentRegistry::registerItem(const CustomItemDefinition &item,
                                                                ItemDefinitionRegistry &items) {
    if (mFrozen || hasIdentifier(item.mIdentifier) || items.getDefinition(item.mIdentifier) != nullptr)
        return nullptr;

    _registerItem(item, items);
    return &mItems.back();
}

const CustomBlockDefinition *CustomContentRegistry::registerBlock(const CustomBlockDefinition &block,
                                                                  ItemDefinitionRegistry &items,
                                                                  BlockDefinitionRegistry &blocks) {
    if (mFrozen || hasIdentifier(block.mIdentifier) || items.getDefinition(block.mIdentifier) != nullptr ||
        blocks.getDefinition(block.mIdentifier) != nullptr)
        return nullptr;

    _registerBlock(block, items, blocks);
    return &mBlocks.back();
}

const CustomActorDefinition *CustomContentRegistry::registerActor(const CustomActorDefinition &actor) {
    if (mFrozen || hasIdentifier(actor.mIdentifier))
        return nullptr;

    mActors.push_back(actor);
    return &mActors.back();
}

void CustomContentRegistry::load(const BehaviorPackManager &packs, ItemDefinitionRegistry &items,
                                 BlockDefinitionRegistry &blocks) {
    for (const BehaviorPack &pack: packs.getPacks()) {
        _loadPackItems(pack.mPath, items);
        _loadPackBlocks(pack.mPath, items, blocks);
        _loadPackActors(pack.mPath);
        _loadPackRecipes(pack.mPath);
    }

}

namespace {
    CustomRecipeIngredient parseIngredient(const json::Value *value) {
        CustomRecipeIngredient ingredient;
        if (value == nullptr) {
            ingredient.mEmpty = true;
            return ingredient;
        }

        if (value->isString()) {
            ingredient.mItemId = value->string();
        } else if (value->isObject()) {
            const json::Value *item = value->get("item");
            if (item != nullptr)
                ingredient.mItemId = item->string();
            const json::Value *count = value->get("count");
            if (count != nullptr)
                ingredient.mCount = count->integer(1);
        }

        ingredient.mEmpty = ingredient.mItemId.empty();
        return ingredient;
    }

    bool parseResult(const json::Value *result, std::string &item, int32_t &count) {
        if (result == nullptr)
            return false;

        const json::Value *object = result;
        if (result->isArray() && !result->mArray.empty())
            object = result->mArray[0].get();

        const json::Value *itemValue = object->get("item");
        if (itemValue == nullptr)
            return false;

        item = itemValue->string();
        const json::Value *countValue = object->get("count");
        count = countValue != nullptr ? countValue->integer(1) : 1;
        return !item.empty();
    }
}

void CustomContentRegistry::_loadPackRecipes(const std::string &packPath) {
    std::error_code error;
    const std::filesystem::path directory = std::filesystem::path(packPath) / "recipes";
    if (!std::filesystem::exists(directory, error))
        return;

    for (const std::filesystem::directory_entry &entry:
                 std::filesystem::recursive_directory_iterator(directory, error)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
            continue;

        const std::string contents = readFile(entry.path());
        std::unique_ptr<json::Value> root = json::parse(contents);
        if (root == nullptr || !root->isObject())
            continue;

        const json::Value *shaped = root->get("minecraft:recipe_shaped");
        const json::Value *shapeless = root->get("minecraft:recipe_shapeless");

        CustomRecipe recipe;

        if (shaped != nullptr && shaped->isObject()) {
            recipe.mShaped = true;

            const json::Value *pattern = shaped->get("pattern");
            const json::Value *key = shaped->get("key");
            if (pattern == nullptr || !pattern->isArray() || key == nullptr)
                continue;

            std::vector<std::string> rows;
            size_t width = 0;
            for (const std::unique_ptr<json::Value> &row: pattern->mArray) {
                const std::string text = row->string();
                rows.push_back(text);
                width = std::max(width, text.size());
            }

            recipe.mWidth = (int32_t) width;
            recipe.mHeight = (int32_t) rows.size();

            for (const std::string &row: rows) {
                for (size_t x = 0; x < width; ++x) {
                    const char symbol = x < row.size() ? row[x] : ' ';
                    if (symbol == ' ') {
                        CustomRecipeIngredient empty;
                        empty.mEmpty = true;
                        recipe.mInputs.push_back(empty);
                        continue;
                    }

                    const std::string keyName(1, symbol);
                    recipe.mInputs.push_back(parseIngredient(key->get(keyName)));
                }
            }

            if (!parseResult(shaped->get("result"), recipe.mResultItem, recipe.mResultCount))
                continue;

            const json::Value *description = shaped->get("description");
            if (description != nullptr && description->get("identifier") != nullptr)
                recipe.mIdentifier = description->get("identifier")->string();
        } else if (shapeless != nullptr && shapeless->isObject()) {
            recipe.mShaped = false;

            const json::Value *ingredients = shapeless->get("ingredients");
            if (ingredients != nullptr && ingredients->isArray()) {
                for (const std::unique_ptr<json::Value> &ingredient: ingredients->mArray)
                    recipe.mInputs.push_back(parseIngredient(ingredient.get()));
            }

            if (!parseResult(shapeless->get("result"), recipe.mResultItem, recipe.mResultCount))
                continue;

            const json::Value *description = shapeless->get("description");
            if (description != nullptr && description->get("identifier") != nullptr)
                recipe.mIdentifier = description->get("identifier")->string();
        } else {
            continue;
        }

        recipe.mResultItem = qualify(recipe.mResultItem);
        mRecipes.push_back(recipe);
    }
}

void CustomContentRegistry::_loadPackActors(const std::string &packPath) {
    std::error_code error;
    const std::filesystem::path directory = std::filesystem::path(packPath) / "entities";
    if (!std::filesystem::exists(directory, error))
        return;

    for (const std::filesystem::directory_entry &entry:
                 std::filesystem::recursive_directory_iterator(directory, error)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
            continue;

        const std::string contents = readFile(entry.path());
        std::unique_ptr<json::Value> root = json::parse(contents);
        if (root == nullptr || !root->isObject())
            continue;

        const json::Value *actor = root->get("minecraft:entity");
        if (actor == nullptr || !actor->isObject())
            continue;

        const json::Value *description = actor->get("description");
        if (description == nullptr)
            continue;

        const json::Value *identifierValue = description->get("identifier");
        if (identifierValue == nullptr || !identifierValue->isString())
            continue;

        CustomActorDefinition definition;
        definition.mIdentifier = identifierValue->string();

        const json::Value *runtimeIdentifier = description->get("runtime_identifier");
        if (runtimeIdentifier != nullptr && runtimeIdentifier->isString())
            definition.mRuntimeIdentifier = runtimeIdentifier->string();

        const json::Value *summonable = description->get("is_summonable");
        if (summonable != nullptr)
            definition.mIsSummonable = summonable->boolean(true);

        const json::Value *spawnable = description->get("is_spawnable");
        if (spawnable != nullptr)
            definition.mIsSpawnable = spawnable->boolean(false);

        const json::Value *experimental = description->get("is_experimental");
        if (experimental != nullptr)
            definition.mIsExperimental = experimental->boolean(false);

        const json::Value *components = actor->get("components");
        if (components != nullptr && components->isObject()) {
            const json::Value *typeFamily = components->get("minecraft:type_family");
            if (typeFamily != nullptr) {
                const json::Value *families = typeFamily->get("family");
                if (families != nullptr && families->isArray()) {
                    for (const std::unique_ptr<json::Value> &family: families->mArray) {
                        if (family->isString())
                            definition.mTypeFamilies.push_back(family->string());
                    }
                }
            }

            if (components->get("minecraft:projectile") != nullptr)
                definition.mIsProjectile = true;

            const json::Value *collision = components->get("minecraft:collision_box");
            if (collision != nullptr && collision->isObject()) {
                if (collision->get("width") != nullptr)
                    definition.mCollisionWidth = (float) collision->get("width")->number(0.6);
                if (collision->get("height") != nullptr)
                    definition.mCollisionHeight = (float) collision->get("height")->number(1.8);
            }
        }

        const json::Value *properties = description->get("properties");
        if (properties != nullptr && properties->isObject()) {
            std::vector<std::string> names;
            for (const auto &pair: properties->mObject)
                names.push_back(pair.first);
            std::sort(names.begin(), names.end());

            int32_t index = 0;
            for (const std::string &name: names) {
                const json::Value *property = properties->get(name);
                ActorPropertyDescription descriptor;
                descriptor.mName = name;
                descriptor.mIndex = index++;

                const std::string type = property->get("type") != nullptr ? property->get("type")->string() : "int";
                const json::Value *range = property->get("range");
                const json::Value *defaultValue = property->get("default");
                descriptor.mClientSync = property->get("client_sync") != nullptr &&
                                         property->get("client_sync")->boolean(false);

                if (type == "bool") {
                    descriptor.mType = ActorPropertyDescription::Type::Bool;
                    descriptor.mDefaultBool = defaultValue != nullptr && defaultValue->boolean(false);
                    descriptor.mDefaultInt = descriptor.mDefaultBool ? 1 : 0;
                    descriptor.mMinInt = 0;
                    descriptor.mMaxInt = 1;
                } else if (type == "float") {
                    descriptor.mType = ActorPropertyDescription::Type::Float;
                    if (range != nullptr && range->isArray() && range->mArray.size() >= 2) {
                        descriptor.mMinFloat = (float) range->mArray[0]->number(0.0);
                        descriptor.mMaxFloat = (float) range->mArray[1]->number(0.0);
                    }
                    descriptor.mDefaultFloat = defaultValue != nullptr ? (float) defaultValue->number(0.0) : 0.0f;
                } else if (type == "enum") {
                    descriptor.mType = ActorPropertyDescription::Type::Enum;
                    const json::Value *values = property->get("values");
                    if (values != nullptr && values->isArray()) {
                        for (const std::unique_ptr<json::Value> &value: values->mArray) {
                            if (value->isString())
                                descriptor.mEnumValues.push_back(value->string());
                        }
                    }
                    descriptor.mMinInt = 0;
                    descriptor.mMaxInt = (int32_t) descriptor.mEnumValues.size() - 1;
                    if (defaultValue != nullptr && defaultValue->isString()) {
                        for (size_t i = 0; i < descriptor.mEnumValues.size(); ++i) {
                            if (descriptor.mEnumValues[i] == defaultValue->string())
                                descriptor.mDefaultInt = (int32_t) i;
                        }
                    }
                } else {
                    descriptor.mType = ActorPropertyDescription::Type::Int;
                    if (range != nullptr && range->isArray() && range->mArray.size() >= 2) {
                        descriptor.mMinInt = range->mArray[0]->integer(0);
                        descriptor.mMaxInt = range->mArray[1]->integer(0);
                    }
                    descriptor.mDefaultInt = defaultValue != nullptr ? defaultValue->integer(0) : 0;
                }

                definition.mProperties.push_back(descriptor);
            }
        }

        bool duplicate = false;
        for (const CustomActorDefinition &existing: mActors) {
            if (existing.mIdentifier == definition.mIdentifier) {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
            continue;

        LOG_TRACE(LogAreaID::Server, "Custom actor %s registered (%zu properties)", definition.mIdentifier.c_str(),
                  definition.mProperties.size());
        mActors.push_back(definition);
    }
}

void CustomContentRegistry::_loadPackItems(const std::string &packPath, ItemDefinitionRegistry &items) {
    std::error_code error;
    const std::filesystem::path directory = std::filesystem::path(packPath) / "items";
    if (!std::filesystem::exists(directory, error))
        return;

    for (const std::filesystem::directory_entry &entry:
                 std::filesystem::recursive_directory_iterator(directory, error)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
            continue;

        const std::string contents = readFile(entry.path());
        std::unique_ptr<json::Value> root = json::parse(contents);
        if (root == nullptr || !root->isObject())
            continue;

        const json::Value *item = root->get("minecraft:item");
        if (item == nullptr || !item->isObject())
            continue;

        const json::Value *description = item->get("description");
        if (description == nullptr)
            continue;

        const json::Value *identifierValue = description->get("identifier");
        if (identifierValue == nullptr || !identifierValue->isString())
            continue;

        CustomItemDefinition definition;
        definition.mIdentifier = qualify(identifierValue->string());
        definition.mDisplayName = prettify(definition.mIdentifier);

        const json::Value *menuCategory = description->get("menu_category");
        if (menuCategory != nullptr && menuCategory->isObject()) {
            if (menuCategory->get("category") != nullptr)
                definition.mCreativeCategory = menuCategory->get("category")->string(definition.mCreativeCategory);
            if (menuCategory->get("group") != nullptr)
                definition.mCreativeGroup = menuCategory->get("group")->string();
        }

        const json::Value *components = item->get("components");
        if (components != nullptr && components->isObject()) {
            const json::Value *icon = components->get("minecraft:icon");
            if (icon != nullptr) {
                if (icon->isString())
                    definition.mIcon = icon->string();
                else if (icon->isObject() && icon->get("texture") != nullptr)
                    definition.mIcon = icon->get("texture")->string();
            }

            const json::Value *displayName = components->get("minecraft:display_name");
            if (displayName != nullptr && displayName->get("value") != nullptr)
                definition.mDisplayName = displayName->get("value")->string(definition.mDisplayName);

            const json::Value *maxStack = components->get("minecraft:max_stack_size");
            if (maxStack != nullptr)
                definition.mMaxStackSize = maxStack->isObject() ? maxStack->get("value")->integer(64)
                                                                : maxStack->integer(64);

            const json::Value *cooldown = components->get("minecraft:cooldown");
            if (cooldown != nullptr && cooldown->isObject()) {
                if (cooldown->get("category") != nullptr)
                    definition.mCooldownCategory = cooldown->get("category")->string();
                if (cooldown->get("duration") != nullptr)
                    definition.mCooldownTicks = (int32_t) (cooldown->get("duration")->number(0.0) * 20.0);
            }

            const json::Value *food = components->get("minecraft:food");
            if (food != nullptr && food->isObject()) {
                definition.mIsFood = true;
                definition.mNutrition = food->get("nutrition") != nullptr ? food->get("nutrition")->integer(0) : 0;
                if (food->get("saturation_modifier") != nullptr)
                    definition.mSaturationModifier = (float) food->get("saturation_modifier")->number(0.6);
                if (food->get("can_always_eat") != nullptr)
                    definition.mCanAlwaysEat = food->get("can_always_eat")->boolean(false);
            }

            const json::Value *durability = components->get("minecraft:durability");
            if (durability != nullptr && durability->isObject() && durability->get("max_durability") != nullptr)
                definition.mMaxDurability = durability->get("max_durability")->integer(0);

            const json::Value *customComponents = components->get("minecraft:custom_components");
            if (customComponents != nullptr && customComponents->isArray()) {
                for (const std::unique_ptr<json::Value> &entry: customComponents->mArray) {
                    if (entry->isString())
                        definition.mCustomComponents.push_back(entry->string());
                }
            }

            const json::Value *digger = components->get("minecraft:digger");
            if (digger != nullptr && digger->isObject()) {
                definition.mHasDigger = true;
                if (digger->get("use_efficiency") != nullptr)
                    definition.mDiggerUseEfficiency = digger->get("use_efficiency")->boolean(false);

                const json::Value *destroySpeeds = digger->get("destroy_speeds");
                if (destroySpeeds != nullptr && destroySpeeds->isArray()) {
                    for (const std::unique_ptr<json::Value> &entry: destroySpeeds->mArray) {
                        if (!entry->isObject())
                            continue;

                        const json::Value *speedValue = entry->get("speed");
                        const int32_t speed = speedValue != nullptr ? speedValue->integer(0) : 0;
                        if (speed <= 0)
                            continue;

                        const json::Value *block = entry->get("block");
                        if (block == nullptr)
                            continue;

                        if (block->isString()) {
                            definition.mDiggerBlockSpeeds[qualify(block->string())] = speed;
                        } else if (block->isObject()) {
                            if (speed > definition.mDiggerToolSpeed)
                                definition.mDiggerToolSpeed = speed;
                        }
                    }
                }
            }
        }

        if (definition.mIcon.empty())
            definition.mIcon = definition.mIdentifier.substr(definition.mIdentifier.find(':') + 1);

        _registerItem(definition, items);
    }
}

void CustomContentRegistry::_registerItem(const CustomItemDefinition &item, ItemDefinitionRegistry &items) {
    for (const CustomItemDefinition &existing: mItems) {
        if (existing.mIdentifier == item.mIdentifier) {
            LOG_WARN(LogAreaID::Server, "Skipping duplicate custom item %s", item.mIdentifier.c_str());
            return;
        }
    }

    CustomItemDefinition stored = item;
    stored.mNetworkId = mNextItemNetworkId++;

    Tag componentData = _buildItemComponentData(stored, items);
    items.registerDefinition(std::make_shared<ItemDefinition>(stored.mIdentifier, stored.mNetworkId, true,
                                                              componentData));

    const std::string identifier = stored.mIdentifier;
    const std::string name = stored.mDisplayName;
    const int32_t networkId = stored.mNetworkId;
    const int32_t maxStackSize = stored.mMaxStackSize;
    StringToItemParser::getInstance().registerItem(identifier, [networkId, identifier, name, maxStackSize]() {
        return Item(networkId, identifier, name, maxStackSize);
    });

    mItems.push_back(stored);
    LOG_TRACE(LogAreaID::Server, "Custom item %s registered (network id %d)", stored.mIdentifier.c_str(),
              stored.mNetworkId);
}

Tag CustomContentRegistry::_buildItemComponentData(const CustomItemDefinition &item, ItemDefinitionRegistry &items) {
    (void) items;

    int32_t creativeCategoryId = 4;
    if (item.mCreativeCategory == "construction")
        creativeCategoryId = 1;
    else if (item.mCreativeCategory == "nature")
        creativeCategoryId = 2;
    else if (item.mCreativeCategory == "equipment")
        creativeCategoryId = 3;

    Tag icon = Tag::ofCompound();
    Tag textures = Tag::ofCompound();
    textures.putString("default", item.mIcon);
    icon.put("textures", textures);

    Tag itemProperties = Tag::ofCompound();
    itemProperties.putInt("creative_category", creativeCategoryId);
    itemProperties.putString("creative_group", item.mCreativeGroup);
    itemProperties.putByte("is_hidden_in_commands", 0);
    itemProperties.putString("enchantable_slot", "none");
    itemProperties.putInt("enchantable_value", 0);
    itemProperties.putBool("allow_off_hand", true);
    itemProperties.putBool("can_destroy_in_creative", true);
    itemProperties.putInt("damage", 0);
    itemProperties.putBool("foil", false);
    itemProperties.putInt("frame_count", 1);
    itemProperties.putBool("hand_equipped", item.mHandEquipped);
    itemProperties.putBool("liquid_clipped", false);
    itemProperties.putInt("max_stack_size", item.mMaxStackSize);
    itemProperties.putFloat("mining_speed", 1.0f);
    itemProperties.putBool("should_despawn", true);
    itemProperties.putBool("stacked_by_data", false);
    itemProperties.putInt("use_duration", 0);
    itemProperties.put("minecraft:icon", icon);

    if (item.mMaxDurability > 0)
        itemProperties.putInt("max_damage", item.mMaxDurability);

    Tag components = Tag::ofCompound();
    components.put("item_properties", itemProperties);

    Tag displayName = Tag::ofCompound();
    displayName.putString("value", item.mDisplayName);
    components.put("minecraft:display_name", displayName);

    Tag maxStack = Tag::ofCompound();
    maxStack.putByte("value", (int8_t) (item.mMaxStackSize > 127 ? 127 : item.mMaxStackSize));
    components.put("minecraft:max_stack_size", maxStack);

    if (item.mIsFood) {
        Tag food = Tag::ofCompound();
        food.putInt("nutrition", item.mNutrition);
        food.putFloat("saturation_modifier", item.mSaturationModifier);
        food.putBool("can_always_eat", item.mCanAlwaysEat);
        components.put("minecraft:food", food);
    }

    if (item.mMaxDurability > 0) {
        Tag durability = Tag::ofCompound();
        durability.putInt("max_durability", item.mMaxDurability);

        Tag damageChance = Tag::ofCompound();
        damageChance.putInt("min", 100);
        damageChance.putInt("max", 100);
        durability.put("damage_chance", damageChance);

        components.put("minecraft:durability", durability);
    }

    Tag definition = Tag::ofCompound();
    definition.putString("name", item.mIdentifier);
    definition.putInt("id", item.mNetworkId);
    definition.put("components", components);
    return definition;
}

void CustomContentRegistry::_loadPackBlocks(const std::string &packPath, ItemDefinitionRegistry &items,
                                            BlockDefinitionRegistry &blocks) {
    std::error_code error;
    const std::filesystem::path directory = std::filesystem::path(packPath) / "blocks";
    if (!std::filesystem::exists(directory, error))
        return;

    for (const std::filesystem::directory_entry &entry:
                 std::filesystem::recursive_directory_iterator(directory, error)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
            continue;

        const std::string contents = readFile(entry.path());
        std::unique_ptr<json::Value> root = json::parse(contents);
        if (root == nullptr || !root->isObject())
            continue;

        const json::Value *block = root->get("minecraft:block");
        if (block == nullptr || !block->isObject())
            continue;

        const json::Value *description = block->get("description");
        if (description == nullptr)
            continue;

        const json::Value *identifierValue = description->get("identifier");
        if (identifierValue == nullptr || !identifierValue->isString())
            continue;

        CustomBlockDefinition definition;
        definition.mIdentifier = qualify(identifierValue->string());
        definition.mDisplayName = prettify(definition.mIdentifier);
        definition.mTexture = definition.mIdentifier.substr(definition.mIdentifier.find(':') + 1);

        const json::Value *description2 = block->get("description");
        if (description2 != nullptr) {
            const json::Value *menuCategory = description2->get("menu_category");
            if (menuCategory != nullptr) {
                if (menuCategory->get("category") != nullptr)
                    definition.mMenuCategory = menuCategory->get("category")->string(definition.mMenuCategory);
                if (menuCategory->get("group") != nullptr)
                    definition.mCreativeGroup = menuCategory->get("group")->string();
            }
        }

        const json::Value *components = block->get("components");
        if (components != nullptr && components->isObject()) {
            const json::Value *destructible = components->get("minecraft:destructible_by_mining");
            if (destructible != nullptr && destructible->isObject() &&
                destructible->get("seconds_to_destroy") != nullptr) {
                definition.mDestroyTime = (float) destructible->get("seconds_to_destroy")->number(0.5);
                definition.mHasDestroyTime = true;
            }

            const json::Value *explosion = components->get("minecraft:destructible_by_explosion");
            if (explosion != nullptr && explosion->isObject() &&
                explosion->get("explosion_resistance") != nullptr) {
                definition.mExplosionResistance = (float) explosion->get("explosion_resistance")->number(1.0);
                definition.mHasExplosionResistance = true;
            }

            const json::Value *light = components->get("minecraft:light_emission");
            if (light != nullptr)
                definition.mLightEmission = light->integer(0);

            const json::Value *friction = components->get("minecraft:friction");
            if (friction != nullptr) {
                definition.mFriction = (float) friction->number(0.6);
                definition.mHasFriction = true;
            }

            const json::Value *material = components->get("minecraft:material_instances");
            if (material != nullptr && material->isObject()) {
                const json::Value *instance = material->get("*");
                if (instance == nullptr)
                    instance = material->get("up");
                if (instance != nullptr && instance->isObject()) {
                    if (instance->get("texture") != nullptr)
                        definition.mTexture = instance->get("texture")->string(definition.mTexture);
                    if (instance->get("render_method") != nullptr)
                        definition.mRenderMethod = instance->get("render_method")->string(definition.mRenderMethod);
                }
            }

            parseBox(components->get("minecraft:collision_box"), definition.mCollision);
            parseBox(components->get("minecraft:selection_box"), definition.mSelection);
        }

        _registerBlock(definition, items, blocks);
    }
}

void CustomContentRegistry::_registerBlock(const CustomBlockDefinition &block, ItemDefinitionRegistry &items,
                                           BlockDefinitionRegistry &blocks) {
    for (const CustomBlockDefinition &existing: mBlocks) {
        if (existing.mIdentifier == block.mIdentifier) {
            LOG_WARN(LogAreaID::Server, "Skipping duplicate custom block %s", block.mIdentifier.c_str());
            return;
        }
    }

    CustomBlockDefinition stored = block;
    stored.mVanillaBlockId = mNextBlockRuntimeId++;
    stored.mItemNetworkId = mNextItemNetworkId++;
    const Tag states = Tag::ofCompound();
    stored.mNetworkHash = BlockStateHasher::hash(stored.mIdentifier, states);

    blocks.registerDefinition(std::make_shared<BlockDefinition>(stored.mIdentifier, stored.mNetworkHash, states));

    int32_t blockCategoryId = 1;
    if (stored.mMenuCategory == "nature")
        blockCategoryId = 2;
    else if (stored.mMenuCategory == "equipment")
        blockCategoryId = 3;
    else if (stored.mMenuCategory == "items")
        blockCategoryId = 4;

    Tag blockItemProperties = Tag::ofCompound();
    blockItemProperties.putInt("creative_category", blockCategoryId);
    blockItemProperties.putString("creative_group", stored.mCreativeGroup);
    blockItemProperties.putByte("is_hidden_in_commands", 0);
    blockItemProperties.putBool("allow_off_hand", false);
    blockItemProperties.putBool("can_destroy_in_creative", true);
    blockItemProperties.putInt("frame_count", 1);
    blockItemProperties.putBool("hand_equipped", false);
    blockItemProperties.putInt("max_stack_size", 64);
    blockItemProperties.putFloat("mining_speed", 1.0f);
    blockItemProperties.putBool("should_despawn", true);
    blockItemProperties.putBool("stacked_by_data", false);

    Tag blockComponents = Tag::ofCompound();
    blockComponents.put("item_properties", blockItemProperties);

    Tag blockDisplayName = Tag::ofCompound();
    blockDisplayName.putString("value", stored.mDisplayName);
    blockComponents.put("minecraft:display_name", blockDisplayName);

    Tag blockItemData = Tag::ofCompound();
    blockItemData.putString("name", stored.mIdentifier);
    blockItemData.putInt("id", stored.mItemNetworkId);
    blockItemData.put("components", blockComponents);

    items.registerDefinition(std::make_shared<ItemDefinition>(stored.mIdentifier, stored.mItemNetworkId, true,
                                                              blockItemData));

    BlockPropertyData property;
    property.mName = stored.mIdentifier;
    property.mProperties = _buildBlockPropertyNbt(stored);
    mBlockProperties.push_back(property);

    StringToItemParser::getInstance().registerBlock(stored.mIdentifier, [stored]() {
        return Block(stored.mNetworkHash, stored.mIdentifier, stored.mDisplayName, Tag::ofCompound());
    });

    mBlocks.push_back(stored);
    LOG_TRACE(LogAreaID::Server, "Custom block %s registered (network hash %d)", stored.mIdentifier.c_str(),
              stored.mNetworkHash);
}

namespace {
    Tag collisionBoxComponent(const CustomBlockBox &box) {
        Tag component = Tag::ofCompound();
        component.putByte("enabled", box.mEnabled ? 1 : 0);

        if (box.mEnabled) {
            const float minX = box.mOrigin[0] + 8.0f;
            const float minY = box.mOrigin[1];
            const float minZ = box.mOrigin[2] + 8.0f;

            Tag entry = Tag::ofCompound();
            entry.putFloat("minX", minX);
            entry.putFloat("minY", minY);
            entry.putFloat("minZ", minZ);
            entry.putFloat("maxX", minX + box.mSize[0]);
            entry.putFloat("maxY", minY + box.mSize[1]);
            entry.putFloat("maxZ", minZ + box.mSize[2]);

            component.put("boxes", Tag::ofList(Tag::Type::Compound, {entry}));
        } else {
            component.put("boxes", Tag::ofList(Tag::Type::Compound));
        }

        return component;
    }

    Tag selectionBoxComponent(const CustomBlockBox &box) {
        Tag component = Tag::ofCompound();
        component.putByte("enabled", box.mEnabled ? 1 : 0);
        component.put("origin", Tag::ofList(Tag::Type::Float,
                                            {Tag::ofFloat(box.mOrigin[0]), Tag::ofFloat(box.mOrigin[1]),
                                             Tag::ofFloat(box.mOrigin[2])}));
        component.put("size", Tag::ofList(Tag::Type::Float,
                                          {Tag::ofFloat(box.mSize[0]), Tag::ofFloat(box.mSize[1]),
                                           Tag::ofFloat(box.mSize[2])}));
        return component;
    }

    Tag fullBlockGeometry() {
        Tag geometry = Tag::ofCompound();
        geometry.put("bone_visibility", Tag::ofCompound());
        geometry.putString("culling", "");
        geometry.putString("culling_layer", "minecraft:culling_layer.undefined");
        geometry.putString("culling_shape", "");
        geometry.putString("identifier", "minecraft:geometry.full_block");
        geometry.putByte("ignoreGeometryForIsSolid", 1);
        geometry.putByte("needsLegacyTopRotation", 0);
        geometry.putByte("useLegacyBlockLightAbsorption", 0);
        geometry.putByte("uv_lock", 0);
        return geometry;
    }

    Tag materialInstancesComponent(const CustomBlockDefinition &block) {
        Tag instance = Tag::ofCompound();
        instance.putFloat("ambient_occlusion", 1.0f);
        instance.putByte("packed_bools", 1);
        instance.putByte("isotropic", 0);
        instance.putString("render_method", block.mRenderMethod);
        instance.putString("texture", block.mTexture);
        instance.putString("tint_method", "none");

        Tag materials = Tag::ofCompound();
        materials.put("*", instance);

        Tag component = Tag::ofCompound();
        component.put("mappings", Tag::ofCompound());
        component.put("materials", materials);
        return component;
    }
}

Tag CustomContentRegistry::_buildBlockPropertyNbt(const CustomBlockDefinition &block) {
    Tag properties = Tag::ofCompound();

    Tag components = Tag::ofCompound();

    Tag frictionComponent = Tag::ofCompound();
    frictionComponent.putFloat("value", block.mHasFriction ? block.mFriction : 0.4f);
    components.put("minecraft:friction", frictionComponent);

    Tag destructibleByExplosion = Tag::ofCompound();
    destructibleByExplosion.putInt("explosion_resistance",
                                   block.mHasExplosionResistance ? (int32_t) block.mExplosionResistance : 0);
    components.put("minecraft:destructible_by_explosion", destructibleByExplosion);

    Tag destructibleByMining = Tag::ofCompound();
    destructibleByMining.putFloat("value", block.mHasDestroyTime ? block.mDestroyTime : 0.0f);
    components.put("minecraft:destructible_by_mining", destructibleByMining);

    Tag lightDampening = Tag::ofCompound();
    lightDampening.putByte("lightLevel", 15);
    components.put("minecraft:light_dampening", lightDampening);

    Tag lightEmission = Tag::ofCompound();
    lightEmission.putByte("emission", (int8_t) block.mLightEmission);
    components.put("minecraft:light_emission", lightEmission);

    components.putString("minecraft:map_color", "#ffffff");

    components.put("minecraft:geometry", fullBlockGeometry());
    components.put("minecraft:material_instances", materialInstancesComponent(block));
    components.put("minecraft:collision_box", collisionBoxComponent(block.mCollision));
    components.put("minecraft:selection_box", selectionBoxComponent(block.mSelection));

    Tag displayName = Tag::ofCompound();
    displayName.putString("value", block.mDisplayName);
    components.put("minecraft:display_name", displayName);

    properties.put("components", components);

    Tag menuCategory = Tag::ofCompound();
    menuCategory.putString("category", block.mMenuCategory);
    menuCategory.putString("group", "");
    menuCategory.putByte("is_hidden_in_commands", 0);
    properties.put("menu_category", menuCategory);

    properties.putInt("molangVersion", 9);

    Tag vanillaBlockData = Tag::ofCompound();
    vanillaBlockData.putInt("block_id", block.mVanillaBlockId);
    vanillaBlockData.putString("material", "dirt");
    properties.put("vanilla_block_data", vanillaBlockData);

    return properties;
}
