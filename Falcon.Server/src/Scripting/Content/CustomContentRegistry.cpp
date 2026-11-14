#include "Scripting/Content/CustomContentRegistry.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Core/Text/StringUtil.h"
#include "World/Item/Item.h"
#include "World/Item/StringToItemParser.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Types/BlockDefinition.h"
#include "Protocol/Types/BlockDefinitionRegistry.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Protocol/Types/ItemDefinitionRegistry.h"
#include "Scripting/BehaviorPackManager.h"
#include "World/Actor/Definition/Molang.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <utility>

namespace {
    // NoteBlockInstrument, in the game's order (26.60 strings)
    const char *const NOTE_BLOCK_INSTRUMENTS[] = {
            "note.harp", "note.bd", "note.snare", "note.hat", "note.bassattack", "note.flute", "note.bell",
            "note.guitar", "note.chime", "note.xylophone", "note.iron_xylophone", "note.cow_bell", "note.didgeridoo",
            "note.bit", "note.banjo", "note.pling", "note.trumpet", "note.trumpet_exposed", "note.trumpet_weathered",
            "note.trumpet_oxidized", "note.zombie", "note.skeleton", "note.creeper", "note.enderdragon",
            "note.witherskeleton", "note.piglin", "note.none"};

    bool isNoteBlockInstrument(const std::string &name) {
        for (const char *instrument: NOTE_BLOCK_INSTRUMENTS) {
            if (name == instrument)
                return true;
        }
        return false;
    }

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

    std::string stringify(const json::Value &value) {
        switch (value.mType) {
            case json::Value::Type::Boolean:
                return value.mBoolean ? "true" : "false";
            case json::Value::Type::Number: {
                std::ostringstream number;
                number.precision(17);
                number << value.mNumber;
                return number.str();
            }
            case json::Value::Type::String:
                return "\"" + json::escape(value.mString) + "\"";
            case json::Value::Type::Array: {
                std::string text = "[";
                for (size_t index = 0; index < value.mArray.size(); ++index)
                    text += (index == 0 ? "" : ",") + stringify(*value.mArray[index]);
                return text + "]";
            }
            case json::Value::Type::Object: {
                std::string text = "{";
                for (size_t index = 0; index < value.mKeys.size(); ++index)
                    text += (index == 0 ? "\"" : ",\"") + json::escape(value.mKeys[index])
                            + "\":" + stringify(*value.get(value.mKeys[index]));
                return text + "}";
            }
            default:
                return "null";
        }
    }

    // the components the client draws and collides with; the block's own, then each permutation's over them
    void applyBlockComponents(const json::Value &components, CustomBlockDefinition &definition) {
        const json::Value *destructible = components.get("minecraft:destructible_by_mining");
        if (destructible != nullptr && destructible->isObject() && destructible->get("seconds_to_destroy") != nullptr) {
            definition.mDestroyTime = (float) destructible->get("seconds_to_destroy")->number(0.5);
            definition.mHasDestroyTime = true;
        }

        const json::Value *explosion = components.get("minecraft:destructible_by_explosion");
        if (explosion != nullptr && explosion->isObject() && explosion->get("explosion_resistance") != nullptr) {
            definition.mExplosionResistance = (float) explosion->get("explosion_resistance")->number(1.0);
            definition.mHasExplosionResistance = true;
        }

        const json::Value *light = components.get("minecraft:light_emission");
        if (light != nullptr)
            definition.mLightEmission = light->integer(0);

        const json::Value *friction = components.get("minecraft:friction");
        if (friction != nullptr) {
            definition.mFriction = (float) friction->number(0.6);
            definition.mHasFriction = true;
        }

        const json::Value *material = components.get("minecraft:material_instances");
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

        parseBox(components.get("minecraft:collision_box"), definition.mCollision);
        parseBox(components.get("minecraft:selection_box"), definition.mSelection);

        const json::Value *geometry = components.get("minecraft:geometry");
        if (geometry != nullptr && geometry->isString())
            definition.mGeometry = geometry->string(definition.mGeometry);
        else if (geometry != nullptr && geometry->isObject() && geometry->get("identifier") != nullptr)
            definition.mGeometry = geometry->get("identifier")->string(definition.mGeometry);

        const json::Value *displayName = components.get("minecraft:display_name");
        if (displayName != nullptr && displayName->isString())
            definition.mDisplayName = displayName->string(definition.mDisplayName);
    }

    // a list of booleans, integers or strings, or {"values": {"min", "max"}} for an integer range; a state the game
    // refuses is left out
    void readStateValues(const std::string &block, const std::string &name, const json::Value &value,
                         std::vector<CustomBlockStateValues> &out) {
        const auto refuse = [&](const char *reason) {
            LOG_WARN(LogAreaID::Script, "Skipping block state %s of %s: %s", name.c_str(), block.c_str(), reason);
        };
        CustomBlockStateValues state;
        state.mName = name;
        if (value.isArray()) {
            if (value.mArray.size() <= 1)
                return refuse("Block State enum array must have more than 1 value");
            if (value.mArray.size() > 16)
                return refuse("Block State enum array must have 16 or less elements");
            const json::Value::Type type = value.mArray.front()->mType;
            for (const std::unique_ptr<json::Value> &entry: value.mArray) {
                if (entry->mType != type)
                    return refuse("Block State enum array values are not all of one type");
                if (entry->mType == json::Value::Type::Boolean)
                    state.mValues.push_back(Tag::ofByte(entry->mBoolean ? 1 : 0));
                else if (entry->isNumber())
                    state.mValues.push_back(Tag::ofInt(entry->integer(0)));
                else if (entry->isString())
                    state.mValues.push_back(Tag::ofString(entry->string()));
                else
                    return refuse("Block State invalid values");
            }
            for (size_t i = 0; i < state.mValues.size(); ++i) {
                for (size_t j = i + 1; j < state.mValues.size(); ++j) {
                    if (state.mValues[i] == state.mValues[j])
                        return refuse(type == json::Value::Type::Boolean
                                              ? "Block State enum array of bools cannot both be the same value"
                                              : "Duplicate values not allowed in property enum");
                }
            }
        } else if (value.isObject() && value.get("values") != nullptr && value.get("values")->isObject()) {
            const json::Value &range = *value.get("values");
            const int64_t min = range.get("min") != nullptr ? range.get("min")->integer(0) : 0;
            const int64_t max = range.get("max") != nullptr ? range.get("max")->integer(0) : 0;
            if (max <= min)
                return refuse("Block State enum range values error. Max must be greater than min");
            if (max - min > 15)
                return refuse("Block State enum range cannot be greater than 16");
            for (int64_t entry = min; entry <= max; ++entry)
                state.mValues.push_back(Tag::ofInt((int32_t) entry));
        }
        if (!state.mValues.empty())
            out.push_back(std::move(state));
    }

    int32_t bitsFor(size_t values) {
        int32_t bits = 0;
        while (((size_t) 1 << bits) < values)
            ++bits;
        return bits;
    }

    // a block's states share 16 bits: a state past them is not applied
    void fitStateBits(const std::string &block, std::vector<CustomBlockStateValues> &states) {
        int32_t total = 0;
        for (const CustomBlockStateValues &state: states)
            total += bitsFor(state.mValues.size());
        if (total <= 16)
            return;
        LOG_WARN(LogAreaID::Script,
                 "Block %s has %u states that use %u bits. The maximum supported number of bits is 16 (2^16 "
                 "permutations). Some specified states may not be applied to this block.",
                 block.c_str(), (unsigned) states.size(), (unsigned) total);
        int32_t used = 0;
        std::vector<CustomBlockStateValues> kept;
        for (CustomBlockStateValues &state: states) {
            const int32_t bits = bitsFor(state.mValues.size());
            if (used + bits > 16)
                continue;
            used += bits;
            kept.push_back(std::move(state));
        }
        states = std::move(kept);
    }

    // 1.x lists names under minecraft:custom_components; 2.x declares each one as a key holding its params
    std::vector<CustomComponentUse> readCustomComponents(const json::Value &components) {
        std::vector<CustomComponentUse> uses;
        const json::Value *legacy = components.get("minecraft:custom_components");
        if (legacy != nullptr && legacy->isArray()) {
            for (const std::unique_ptr<json::Value> &entry: legacy->mArray) {
                if (entry->isString())
                    uses.push_back({entry->string(), std::string()});
            }
        }
        for (const std::string &key: components.mKeys) {
            if (key.find(':') == std::string::npos || key.compare(0, 10, "minecraft:") == 0)
                continue;
            uses.push_back({key, stringify(*components.get(key))});
        }
        return uses;
    }

}

CustomContentRegistry &CustomContentRegistry::getInstance() {
    static CustomContentRegistry instance;
    return instance;
}

void CustomContentRegistry::reset() {
    *this = CustomContentRegistry();
}

bool CustomContentRegistry::isCustomBlock(const std::string &identifier) const {
    for (const CustomBlockDefinition &block: mBlocks) {
        if (block.mIdentifier == identifier)
            return true;
    }
    return false;
}

const CustomBlockDefinition *CustomContentRegistry::findBlock(const std::string &identifier) const {
    for (const CustomBlockDefinition &block: mBlocks) {
        if (block.mIdentifier == identifier)
            return &block;
    }
    return nullptr;
}

std::vector<Tag> CustomContentRegistry::permutationsOf(const CustomBlockDefinition &block) {
    std::vector<Tag> permutations(1, Tag::ofCompound());
    for (const CustomBlockStateValues &state: block.mStates) {
        std::vector<Tag> expanded;
        for (const Tag &base: permutations) {
            for (const Tag &value: state.mValues) {
                Tag next = base;
                next.put(state.mName, value);
                expanded.push_back(std::move(next));
            }
        }
        permutations = std::move(expanded);
    }
    return permutations;
}

Tag CustomContentRegistry::defaultStatesOf(const CustomBlockDefinition &block) {
    Tag states = Tag::ofCompound();
    for (const CustomBlockStateValues &state: block.mStates) {
        if (!state.mValues.empty())
            states.put(state.mName, state.mValues.front());
    }
    return states;
}

std::vector<CustomComponentUse> CustomContentRegistry::componentsFor(const CustomBlockDefinition &block,
                                                                     const Tag &states) {
    std::vector<CustomComponentUse> uses = block.mCustomComponents;
    for (const CustomBlockPermutation &permutation: block.mPermutations) {
        if (permutation.mCustomComponents.empty() || !Molang::evaluateForBlock(permutation.mCondition, states).isTrue())
            continue;
        for (const CustomComponentUse &use: permutation.mCustomComponents) {
            const auto same = std::find_if(uses.begin(), uses.end(),
                                           [&](const CustomComponentUse &other) { return other.mName == use.mName; });
            if (same != uses.end())
                *same = use;
            else
                uses.push_back(use);
        }
    }
    return uses;
}

const ActorPropertyDescription *CustomActorDefinition::findProperty(const std::string &name) const {
    return ActorPropertySchema::find(mProperties, name);
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

const CustomItemDefinition *CustomContentRegistry::findItem(const std::string &identifier) const {
    for (const CustomItemDefinition &item: mItems) {
        if (item.mIdentifier == identifier)
            return &item;
    }
    return nullptr;
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
    if (mFrozen || hasIdentifier(block.mIdentifier) || items.getDefinition(block.mIdentifier) != nullptr
        || blocks.getDefinition(block.mIdentifier) != nullptr)
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
    // neighbor_directions: an array of down, up, north, south, west, east (0 to 5) or all
    bool readNeighborDirections(const json::Value &component, uint8_t &directions) {
        static const char *const NAMES[] = {"down", "up", "north", "south", "west", "east"};
        directions = 0;
        if (!component.isObject())
            return false;
        const json::Value *list = component.get("neighbor_directions");
        if (list == nullptr)
            return true;
        if (!list->isArray())
            return false;
        for (const std::unique_ptr<json::Value> &value: list->mArray) {
            if (!value->isString())
                return false;
            const std::string name = value->string();
            if (name == "all") {
                directions = 0x3F;
                continue;
            }
            int face = 0;
            while (face < 6 && name != NAMES[face])
                ++face;
            if (face == 6)
                return false;
            directions |= (uint8_t) (1 << face);
        }
        return true;
    }

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

            const json::Value *projectile = components->get("minecraft:projectile");
            if (projectile != nullptr) {
                definition.mIsProjectile = true;
                definition.mHasProjectileComponent = true;
                definition.mProjectileComponent = ProjectileComponent::read(*projectile);
            }

            const json::Value *collision = components->get("minecraft:collision_box");
            if (collision != nullptr && collision->isObject()) {
                if (collision->get("width") != nullptr)
                    definition.mCollisionWidth = (float) collision->get("width")->number(0.6);
                if (collision->get("height") != nullptr)
                    definition.mCollisionHeight = (float) collision->get("height")->number(1.8);
            }
        }

        const json::Value *properties = description->get("properties");
        if (properties != nullptr)
            definition.mProperties = ActorPropertySchema::parse(*properties);

        bool duplicate = false;
        for (const CustomActorDefinition &existing: mActors) {
            if (existing.mIdentifier == definition.mIdentifier) {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
            continue;

        LOG_TRACE(LogAreaID::Script, "Custom actor %s registered (%zu properties)", definition.mIdentifier.c_str(),
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

            // the game takes these as the value itself or wrapped in an object
            const json::Value *handEquipped = components->get("minecraft:hand_equipped");
            if (handEquipped != nullptr && handEquipped->isObject())
                handEquipped = handEquipped->get("value");
            if (handEquipped != nullptr)
                definition.mHandEquipped = handEquipped->boolean(false);

            const std::pair<const char *, bool *> flags[] = {
                    {"minecraft:allow_off_hand", &definition.mAllowOffHand},
                    {"minecraft:can_destroy_in_creative", &definition.mCanDestroyInCreative},
                    {"minecraft:glint", &definition.mGlint},
                    {"minecraft:liquid_clipped", &definition.mLiquidClipped},
                    {"minecraft:should_despawn", &definition.mShouldDespawn},
                    {"minecraft:stacked_by_data", &definition.mStackedByData},
                    {"minecraft:fire_resistant", &definition.mFireResistant},
            };
            for (const auto &flag: flags) {
                const json::Value *value = components->get(flag.first);
                if (value != nullptr && value->isObject())
                    value = value->get("value");
                if (value != nullptr)
                    *flag.second = value->boolean(*flag.second);
            }

            const json::Value *damage = components->get("minecraft:damage");
            if (damage != nullptr && damage->isObject())
                damage = damage->get("value");
            if (damage != nullptr)
                definition.mDamage = damage->integer(0);

            const json::Value *tags = components->get("minecraft:tags");
            if (tags != nullptr && tags->isObject())
                tags = tags->get("tags");
            if (tags != nullptr && tags->isArray()) {
                for (const std::unique_ptr<json::Value> &tag: tags->mArray) {
                    if (tag->isString()
                        && std::find(definition.mTags.begin(), definition.mTags.end(), tag->string())
                                   == definition.mTags.end())
                        definition.mTags.push_back(tag->string());
                }
            }

            const json::Value *durability = components->get("minecraft:durability");
            if (durability != nullptr && durability->isObject() && durability->get("max_durability") != nullptr)
                definition.mMaxDurability = durability->get("max_durability")->integer(0);

            const json::Value *enchantable = components->get("minecraft:enchantable");
            if (enchantable != nullptr && enchantable->isObject()) {
                definition.mHasEnchantable = true;
                if (enchantable->get("value") != nullptr)
                    definition.mEnchantableValue = enchantable->get("value")->integer(0);
                if (enchantable->get("slot") != nullptr)
                    definition.mEnchantableSlot = StringUtil::toLowerCase(enchantable->get("slot")->string("none"));
            }

            // minecraft:repairable: each repair_items entry lists items (names or {"name": ...})
            // and a repair_amount: number or Molang expression
            const json::Value *repairable = components->get("minecraft:repairable");
            const json::Value *repairItems = repairable != nullptr && repairable->isObject()
                                                     ? repairable->get("repair_items")
                                                     : nullptr;
            if (repairItems != nullptr && repairItems->isArray()) {
                for (const std::unique_ptr<json::Value> &entry: repairItems->mArray) {
                    if (!entry->isObject())
                        continue;

                    CustomRepairEntry repair;
                    const json::Value *itemList = entry->get("items");
                    if (itemList != nullptr && itemList->isArray()) {
                        for (const std::unique_ptr<json::Value> &item: itemList->mArray) {
                            if (item->isString())
                                repair.mItems.push_back(qualify(item->string()));
                            else if (item->isObject() && item->get("name") != nullptr)
                                repair.mItems.push_back(qualify(item->get("name")->string()));
                        }
                    }

                    const json::Value *amount = entry->get("repair_amount");
                    if (amount != nullptr && amount->isString())
                        repair.mAmount = amount->string();
                    else if (amount != nullptr && amount->isNumber())
                        repair.mAmount = std::to_string(amount->number(0.0));
                    else if (amount != nullptr && amount->isObject() && amount->get("expression") != nullptr)
                        repair.mAmount = amount->get("expression")->string();

                    if (!repair.mItems.empty() && !repair.mAmount.empty())
                        definition.mRepairItems.push_back(std::move(repair));
                }
            }

            definition.mCustomComponents = readCustomComponents(*components);

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
            LOG_WARN(LogAreaID::Script, "Skipping duplicate custom item %s", item.mIdentifier.c_str());
            return;
        }
    }

    CustomItemDefinition stored = item;
    stored.mNetworkId = mNextItemNetworkId++;

    Tag componentData = _buildItemComponentData(stored, items);
    items.registerDefinition(
            std::make_shared<ItemDefinition>(stored.mIdentifier, stored.mNetworkId, true, componentData));

    const std::string identifier = stored.mIdentifier;
    const std::string name = stored.mDisplayName;
    const int32_t networkId = stored.mNetworkId;
    const int32_t maxStackSize = stored.mMaxStackSize;
    const int32_t damage = stored.mDamage;
    StringToItemParser::getInstance().registerItem(identifier, [networkId, identifier, name, maxStackSize, damage]() {
        Item item(networkId, identifier, name, maxStackSize);
        item.getComponent<DamageItemComponent>()->mDamage = damage;
        return item;
    });

    mItems.push_back(stored);
    LOG_TRACE(LogAreaID::Script, "Custom item %s registered (network id %d)", stored.mIdentifier.c_str(),
              stored.mNetworkId);
}

namespace {
    // LeafSupportingInFirstSolidBlockBelow, one below NumValidVersions
    const int16_t MOLANG_VERSION_LATEST = 12;
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
    itemProperties.putString("enchantable_slot", item.mEnchantableSlot);
    itemProperties.putInt("enchantable_value", item.mEnchantableValue);
    itemProperties.putBool("allow_off_hand", item.mAllowOffHand);
    itemProperties.putBool("can_destroy_in_creative", item.mCanDestroyInCreative);
    itemProperties.putInt("damage", item.mDamage);
    itemProperties.putBool("foil", item.mGlint);
    itemProperties.putInt("frame_count", 1);
    itemProperties.putBool("hand_equipped", item.mHandEquipped);
    itemProperties.putBool("liquid_clipped", item.mLiquidClipped);
    itemProperties.putInt("max_stack_size", item.mMaxStackSize);
    itemProperties.putFloat("mining_speed", 1.0f);
    itemProperties.putBool("should_despawn", item.mShouldDespawn);
    itemProperties.putBool("stacked_by_data", item.mStackedByData);
    itemProperties.putInt("use_duration", 0);
    itemProperties.put("minecraft:icon", icon);

    if (item.mMaxDurability > 0)
        itemProperties.putInt("max_damage", item.mMaxDurability);

    Tag components = Tag::ofCompound();
    components.put("item_properties", itemProperties);

    // the client reads its item tags from item_tags; the game sends it even empty
    Tag itemTags = Tag::ofList(Tag::Type::String);
    for (const std::string &tag: item.mTags)
        itemTags.addToList(Tag::ofString(tag));
    components.put("item_tags", itemTags);

    if (!item.mTags.empty()) {
        Tag tagsComponent = Tag::ofCompound();
        tagsComponent.put("tags", itemTags);
        components.put("minecraft:tags", tagsComponent);
    }

    if (item.mHandEquipped) {
        Tag handEquipped = Tag::ofCompound();
        handEquipped.putBool("value", true);
        components.put("minecraft:hand_equipped", handEquipped);
    }

    // the one flag the game keeps as a component rather than an item property
    if (item.mFireResistant) {
        Tag fireResistant = Tag::ofCompound();
        fireResistant.putBool("value", true);
        components.put("minecraft:fire_resistant", fireResistant);
    }

    if (item.mDamage != 0) {
        Tag damage = Tag::ofCompound();
        damage.putShort("value", (int16_t) item.mDamage);
        components.put("minecraft:damage", damage);
    }

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

    // the game writes value as a byte here, an int in item_properties
    if (item.mHasEnchantable) {
        Tag enchantable = Tag::ofCompound();
        enchantable.putString("slot", item.mEnchantableSlot);
        enchantable.putByte("value", (int8_t) item.mEnchantableValue);
        components.put("minecraft:enchantable", enchantable);
    }

    // the client previews anvil repairs from this: sent as the game writes it, each repair_amount an ExpressionNode
    // in object form: expression plus short MolangVersion, Latest (12) for pack content
    if (!item.mRepairItems.empty()) {
        Tag repairItems = Tag::ofList(Tag::Type::Compound);
        for (const CustomRepairEntry &entry: item.mRepairItems) {
            Tag items = Tag::ofList(Tag::Type::Compound);
            for (const std::string &name: entry.mItems) {
                Tag descriptor = Tag::ofCompound();
                descriptor.putString("name", name);
                items.addToList(descriptor);
            }

            Tag amount = Tag::ofCompound();
            amount.putString("expression", entry.mAmount);
            amount.putShort("version", MOLANG_VERSION_LATEST);

            Tag repair = Tag::ofCompound();
            repair.put("items", items);
            repair.put("repair_amount", amount);
            repairItems.addToList(repair);
        }

        Tag repairable = Tag::ofCompound();
        repairable.put("repair_items", repairItems);
        components.put("minecraft:repairable", repairable);
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
            const json::Value *states = description2->get("states");
            if (states != nullptr && states->isObject()) {
                for (const std::string &name: states->mKeys)
                    readStateValues(definition.mIdentifier, name, *states->get(name), definition.mStates);
                fitStateBits(definition.mIdentifier, definition.mStates);
            }
        }

        const json::Value *permutations = block->get("permutations");
        if (permutations != nullptr && permutations->isArray()) {
            for (const std::unique_ptr<json::Value> &entry: permutations->mArray) {
                const json::Value *condition = entry->isObject() ? entry->get("condition") : nullptr;
                if (condition == nullptr || !condition->isString())
                    continue;
                CustomBlockPermutation permutation;
                permutation.mCondition = condition->string();
                const json::Value *permutationComponents = entry->get("components");
                if (permutationComponents != nullptr && permutationComponents->isObject()) {
                    permutation.mCustomComponents = readCustomComponents(*permutationComponents);
                    permutation.mComponents = stringify(*permutationComponents);
                }
                definition.mPermutations.push_back(std::move(permutation));
            }
        }

        const json::Value *components = block->get("components");
        if (components != nullptr && components->isObject()) {
            definition.mCustomComponents = readCustomComponents(*components);

            const json::Value *consumer = components->get("minecraft:redstone_consumer");
            // both descriptions start zeroed, min_power 0, min_fall_distance 0
            if (consumer != nullptr && consumer->isObject()) {
                definition.mRedstoneConsumer = true;
                if (consumer->get("min_power") != nullptr) {
                    // outside 0-15 is error 64; the block loader's mask drops the block
                    const double power = consumer->get("min_power")->number(0.0);
                    if (power < 0.0 || power > 15.0) {
                        LOG_WARN(LogAreaID::Script, "Skipping custom block %s: min_power outside 0-15",
                                 definition.mIdentifier.c_str());
                        continue;
                    }
                    definition.mRedstoneMinPower = (int32_t) power;
                }
            }

            const json::Value *instrument = components->get("minecraft:instrument_sound");
            if (instrument != nullptr && instrument->isObject()) {
                definition.mInstrumentSound = true;
                for (const std::string &face: instrument->mKeys) {
                    const json::Value *sound = instrument->get(face);
                    if ((face == "up" || face == "down") && sound != nullptr && sound->isString()
                        && isNoteBlockInstrument(sound->string()))
                        definition.mInstrumentSounds.emplace_back(face, sound->string());
                }
            }

            const json::Value *fallOn = components->get("minecraft:entity_fall_on");
            if (fallOn != nullptr && fallOn->isObject()) {
                definition.mEntityFallOn = true;
                if (fallOn->get("min_fall_distance") != nullptr)
                    definition.mMinFallDistance = (float) fallOn->get("min_fall_distance")->number(0.0);
            }

            const json::Value *tick = components->get("minecraft:tick");
            if (tick != nullptr && tick->isObject()) {
                definition.mHasTick = true;
                const json::Value *range = tick->get("interval_range");
                if (range != nullptr && range->isArray() && range->mArray.size() == 2) {
                    definition.mTickMin = std::max(1, (int32_t) range->mArray[0]->integer(10));
                    definition.mTickMax = std::max(definition.mTickMin, (int32_t) range->mArray[1]->integer(10));
                }
                if (tick->get("looping") != nullptr)
                    definition.mTickLooping = tick->get("looping")->boolean(true);
            }

            const json::Value *neighbor = components->get("minecraft:neighbor_change");
            if (neighbor != nullptr) {
                // BlockDefinitionNeighborChangeValidate: the document fails without the experiment
                if (!mUpcomingCreatorFeatures) {
                    LOG_ERROR(LogAreaID::Script,
                              "To use 'minecraft:neighbor_change', experimental creator features are required");
                    continue;
                }
                if (!readNeighborDirections(*neighbor, definition.mNeighborDirections)) {
                    LOG_WARN(LogAreaID::Script, "Skipping custom block %s: invalid neighbor_directions",
                             definition.mIdentifier.c_str());
                    continue;
                }
            }

            const json::Value *blockEntity = components->get("minecraft:block_entity");
            if (blockEntity != nullptr && blockEntity->isObject()) {
                // container and entity_storage need the experiment, the document fails without it
                const json::Value *storage = blockEntity->get("entity_storage");
                const bool storesEntities = storage != nullptr && storage->boolean(false);
                if (!mUpcomingCreatorFeatures && blockEntity->get("container") != nullptr) {
                    LOG_ERROR(LogAreaID::Script, "To use 'container' in 'minecraft:block_entity', experimental "
                                                 "creator features are required");
                    continue;
                }
                if (!mUpcomingCreatorFeatures && storesEntities) {
                    LOG_ERROR(LogAreaID::Script, "To use 'entity_storage' in 'minecraft:block_entity', experimental "
                                                 "creator features are required");
                    continue;
                }
                definition.mEntityStorage = storesEntities;
            }

            const json::Value *vibration = components->get("minecraft:vibration_properties");
            if (vibration != nullptr) {
                // the document fails without the experiment
                if (!mUpcomingCreatorFeatures) {
                    LOG_ERROR(LogAreaID::Script,
                              "To use 'minecraft:vibration_properties', experimental creator features are required");
                    continue;
                }
                const json::Value *dampen = vibration->get("can_dampen_vibrations");
                if (dampen != nullptr)
                    definition.mCanDampenVibrations = dampen->boolean(false);
                const json::Value *occlude = vibration->get("can_occlude_vibrations");
                if (occlude != nullptr)
                    definition.mCanOccludeVibrations = occlude->boolean(false);
            }

            applyBlockComponents(*components, definition);
            const json::Value *placement = components->get("minecraft:placement_filter");
            if (placement != nullptr && placement->isObject())
                definition.mPlacementFilter = BlockPlacementFilter::parse(*placement, definition.mIdentifier);
        }

        _registerBlock(definition, items, blocks);
    }
}

void CustomContentRegistry::_registerBlock(const CustomBlockDefinition &block, ItemDefinitionRegistry &items,
                                           BlockDefinitionRegistry &blocks) {
    for (const CustomBlockDefinition &existing: mBlocks) {
        if (existing.mIdentifier == block.mIdentifier) {
            LOG_WARN(LogAreaID::Script, "Skipping duplicate custom block %s", block.mIdentifier.c_str());
            return;
        }
    }

    CustomBlockDefinition stored = block;
    stored.mVanillaBlockId = mNextBlockRuntimeId++;
    stored.mItemNetworkId = mNextItemNetworkId++;
    const Tag states = defaultStatesOf(stored);
    stored.mNetworkHash = BlockStateHasher::hash(stored.mIdentifier, states);
    if (stored.mNeighborDirections != 0)
        mNeighborListeners[stored.mIdentifier] = stored.mNeighborDirections;

    // hashed runtime ids: one definition per permutation, the default first
    blocks.registerDefinition(std::make_shared<BlockDefinition>(stored.mIdentifier, stored.mNetworkHash, states));
    for (const Tag &permutation: permutationsOf(stored)) {
        if (!(permutation == states))
            blocks.registerDefinition(std::make_shared<BlockDefinition>(
                    stored.mIdentifier, BlockStateHasher::hash(stored.mIdentifier, permutation), permutation));
    }

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

    items.registerDefinition(
            std::make_shared<ItemDefinition>(stored.mIdentifier, stored.mItemNetworkId, true, blockItemData));

    BlockPropertyData property;
    property.mName = stored.mIdentifier;
    property.mProperties = _buildBlockPropertyNbt(stored);
    mBlockProperties.push_back(property);

    StringToItemParser::getInstance().registerBlock(stored.mIdentifier, [stored, states]() {
        return Block(stored.mNetworkHash, stored.mIdentifier, stored.mDisplayName, states);
    });

    mBlocks.push_back(stored);
    LOG_TRACE(LogAreaID::Script, "Custom block %s registered (network hash %d)", stored.mIdentifier.c_str(),
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
        component.put("origin",
                      Tag::ofList(Tag::Type::Float, {Tag::ofFloat(box.mOrigin[0]), Tag::ofFloat(box.mOrigin[1]),
                                                     Tag::ofFloat(box.mOrigin[2])}));
        component.put("size", Tag::ofList(Tag::Type::Float, {Tag::ofFloat(box.mSize[0]), Tag::ofFloat(box.mSize[1]),
                                                             Tag::ofFloat(box.mSize[2])}));
        return component;
    }

    Tag geometryComponent(const std::string &identifier) {
        Tag geometry = Tag::ofCompound();
        geometry.put("bone_visibility", Tag::ofCompound());
        geometry.putString("culling", "");
        geometry.putString("culling_layer", "minecraft:culling_layer.undefined");
        geometry.putString("culling_shape", "");
        geometry.putString("identifier", identifier);
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

namespace {
    // the client NBT of a definition's components
    Tag clientComponents(const CustomBlockDefinition &block) {
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

        components.put("minecraft:geometry", geometryComponent(block.mGeometry));
        components.put("minecraft:material_instances", materialInstancesComponent(block));
        components.put("minecraft:collision_box", collisionBoxComponent(block.mCollision));
        components.put("minecraft:selection_box", selectionBoxComponent(block.mSelection));

        Tag displayName = Tag::ofCompound();
        displayName.putString("value", block.mDisplayName);
        components.put("minecraft:display_name", displayName);
        return components;
    }

    // a permutation sends only what it declares, in the client form of the block's own components
    Tag permutationComponents(const CustomBlockDefinition &block, const std::string &declared) {
        Tag components = Tag::ofCompound();
        const std::unique_ptr<json::Value> json = json::parse(declared);
        if (json == nullptr || !json->isObject())
            return components;

        CustomBlockDefinition overlay = block;
        applyBlockComponents(*json, overlay);
        const Tag full = clientComponents(overlay);
        for (const std::string &key: json->mKeys) {
            const Tag *component = full.get(key);
            if (component != nullptr)
                components.put(key, *component);
        }
        return components;
    }
}

Tag CustomContentRegistry::_buildBlockPropertyNbt(const CustomBlockDefinition &block) {
    Tag properties = Tag::ofCompound();
    const Tag components = clientComponents(block);
    properties.put("components", components);

    // properties [{name, enum}], permutations [{condition, components}]
    if (!block.mStates.empty()) {
        Tag states = Tag::ofList(Tag::Type::Compound);
        for (const CustomBlockStateValues &state: block.mStates) {
            Tag property = Tag::ofCompound();
            property.putString("name", state.mName);
            property.put("enum", Tag::ofList(state.mValues.front().getType(), state.mValues));
            states.addToList(property);
        }
        properties.put("properties", states);
    }
    if (!block.mPermutations.empty()) {
        Tag permutations = Tag::ofList(Tag::Type::Compound);
        for (const CustomBlockPermutation &permutation: block.mPermutations) {
            Tag entry = Tag::ofCompound();
            entry.putString("condition", permutation.mCondition);
            entry.put("components", permutationComponents(block, permutation.mComponents));
            permutations.addToList(entry);
        }
        properties.put("permutations", permutations);
    }

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
