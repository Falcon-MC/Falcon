#include "Scripting/Binding/ScriptApi.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Inventory/Container.h"
#include "World/Level/Block/Actor/BlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/Actor/PistonArmBlockActor.h"
#include "World/Level/Block/Actor/SignBlockActor.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/BlockLightProperties.h"
#include "World/Level/Block/BlockPickItem.h"
#include "World/Level/Block/BlockPrecipitation.h"
#include "World/Level/Block/BlockTags.h"
#include "World/Level/Block/CauldronBlock.h"
#include "World/Level/Block/LiquidProperties.h"
#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"
#include "World/Level/Block/Systems/PistonSystem.h"
#include "World/Level/Block/Systems/PrecipitationSystem.h"
#include "World/Level/Block/Actor/CauldronBlockActor.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"
#include "World/Level/Map/MapColors.h"
#include "World/Level/Block/BlockNames.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <cmath>
#include <unordered_map>

using namespace ScriptValues;

namespace {
    // BlockBoundingBoxUtils (sub_14DD4B0F0): a box is two block positions; vectors given to the utils truncate toward
    // zero (cvttps), the corners read as block positions, which BlockPos(Vec3) floors
    struct BlockBox {
        int32_t mMin[3];
        int32_t mMax[3];
    };

    bool readTruncated(JSContext *ctx, JSValueConst value, int32_t out[3]) {
        double x = 0.0, y = 0.0, z = 0.0;
        if (!readVector3(ctx, value, x, y, z) || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            return false;
        out[0] = (int32_t) (float) x;
        out[1] = (int32_t) (float) y;
        out[2] = (int32_t) (float) z;
        return true;
    }

    bool readCorner(JSContext *ctx, JSValueConst box, const char *key, int32_t out[3]) {
        JSValue corner = JS_GetPropertyStr(ctx, box, key);
        double x = 0.0, y = 0.0, z = 0.0;
        const bool ok = readVector3(ctx, corner, x, y, z) && std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
        JS_FreeValue(ctx, corner);
        if (!ok)
            return false;
        out[0] = (int32_t) std::floor(x);
        out[1] = (int32_t) std::floor(y);
        out[2] = (int32_t) std::floor(z);
        return true;
    }

    bool readBox(JSContext *ctx, JSValueConst value, BlockBox &box) {
        return JS_IsObject(value) && readCorner(ctx, value, "min", box.mMin) && readCorner(ctx, value, "max", box.mMax);
    }

    JSValue makeBox(JSContext *ctx, const BlockBox &box) {
        JSValue object = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, object, "min",
                          vector3(ctx, (float) box.mMin[0], (float) box.mMin[1], (float) box.mMin[2]));
        JS_SetPropertyStr(ctx, object, "max",
                          vector3(ctx, (float) box.mMax[0], (float) box.mMax[1], (float) box.mMax[2]));
        return object;
    }

    bool boxesIntersect(const BlockBox &a, const BlockBox &b) {
        for (int axis = 0; axis < 3; ++axis) {
            if (b.mMax[axis] < a.mMin[axis] || b.mMin[axis] > a.mMax[axis])
                return false;
        }
        return true;
    }

    JSValue boxCreateValid(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        int32_t a[3], b[3];
        if (argc < 2 || !readTruncated(ctx, argv[0], a) || !readTruncated(ctx, argv[1], b))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        BlockBox box;
        for (int axis = 0; axis < 3; ++axis) {
            box.mMin[axis] = std::min(a[axis], b[axis]);
            box.mMax[axis] = std::max(a[axis], b[axis]);
        }
        return makeBox(ctx, box);
    }

    // sub_14DD50D40: the corners move out by the truncated size, then each axis is put back in order
    JSValue boxDilate(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        BlockBox box;
        int32_t size[3];
        if (argc < 2 || !readBox(ctx, argv[0], box) || !readTruncated(ctx, argv[1], size))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        BlockBox out;
        for (int axis = 0; axis < 3; ++axis) {
            const int32_t low = box.mMin[axis] - size[axis];
            const int32_t high = box.mMax[axis] + size[axis];
            out.mMin[axis] = std::min(low, high);
            out.mMax[axis] = std::max(low, high);
        }
        return makeBox(ctx, out);
    }

    JSValue boxEquals(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        BlockBox a, b;
        if (argc < 2 || !readBox(ctx, argv[0], a) || !readBox(ctx, argv[1], b))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        bool same = true;
        for (int axis = 0; axis < 3; ++axis)
            same = same && a.mMin[axis] == b.mMin[axis] && a.mMax[axis] == b.mMax[axis];
        return JS_NewBool(ctx, same);
    }

    JSValue boxExpand(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        BlockBox a, b;
        if (argc < 2 || !readBox(ctx, argv[0], a) || !readBox(ctx, argv[1], b))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        BlockBox out;
        for (int axis = 0; axis < 3; ++axis) {
            out.mMin[axis] = std::min(a.mMin[axis], b.mMin[axis]);
            out.mMax[axis] = std::max(a.mMax[axis], b.mMax[axis]);
        }
        return makeBox(ctx, out);
    }

    // sub_14DD50E50: whole blocks, the half span rounds toward zero
    JSValue boxGetCenter(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        BlockBox box;
        if (argc < 1 || !readBox(ctx, argv[0], box))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        int32_t center[3];
        for (int axis = 0; axis < 3; ++axis)
            center[axis] = box.mMin[axis] + (box.mMax[axis] - box.mMin[axis] + 1) / 2;
        return vector3(ctx, (float) center[0], (float) center[1], (float) center[2]);
    }

    JSValue boxGetIntersection(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        BlockBox a, b;
        if (argc < 2 || !readBox(ctx, argv[0], a) || !readBox(ctx, argv[1], b))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (!boxesIntersect(a, b))
            return JS_UNDEFINED;
        BlockBox out;
        for (int axis = 0; axis < 3; ++axis) {
            out.mMin[axis] = std::max(a.mMin[axis], b.mMin[axis]);
            out.mMax[axis] = std::min(a.mMax[axis], b.mMax[axis]);
        }
        return makeBox(ctx, out);
    }

    // sub_14DD50E20: both corners count
    JSValue boxGetSpan(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        BlockBox box;
        if (argc < 1 || !readBox(ctx, argv[0], box))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return vector3(ctx, (float) (box.mMax[0] - box.mMin[0] + 1), (float) (box.mMax[1] - box.mMin[1] + 1),
                       (float) (box.mMax[2] - box.mMin[2] + 1));
    }

    JSValue boxIntersects(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        BlockBox a, b;
        if (argc < 2 || !readBox(ctx, argv[0], a) || !readBox(ctx, argv[1], b))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return JS_NewBool(ctx, boxesIntersect(a, b));
    }

    JSValue boxIsInside(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        BlockBox box;
        int32_t at[3];
        if (argc < 2 || !readBox(ctx, argv[0], box) || !readTruncated(ctx, argv[1], at))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        bool inside = true;
        for (int axis = 0; axis < 3; ++axis)
            inside = inside && at[axis] >= box.mMin[axis] && at[axis] <= box.mMax[axis];
        return JS_NewBool(ctx, inside);
    }

    JSValue boxIsValid(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        BlockBox box;
        if (argc < 1 || !readBox(ctx, argv[0], box))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return JS_NewBool(ctx, box.mMax[0] >= box.mMin[0] && box.mMax[1] >= box.mMin[1] && box.mMax[2] >= box.mMin[2]);
    }

    // sub_14DD50DC0
    JSValue boxTranslate(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        BlockBox box;
        int32_t delta[3];
        if (argc < 2 || !readBox(ctx, argv[0], box) || !readTruncated(ctx, argv[1], delta))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        for (int axis = 0; axis < 3; ++axis) {
            box.mMin[axis] += delta[axis];
            box.mMax[axis] += delta[axis];
        }
        return makeBox(ctx, box);
    }

    // Direction values in RedstoneFace order
    const char *const DIRECTIONS[] = {"Down", "Up", "North", "South", "West", "East"};

    JSValue rgba(JSContext *ctx, const float color[4]);

    int faceOf(const std::string &direction) {
        for (int face = 0; face < 6; ++face) {
            if (direction == DIRECTIONS[face])
                return face;
        }
        return -1;
    }

    struct BlockAt {
        ScriptApi *mApi = nullptr;
        Level *mLevel = nullptr;
        Vector3i mPosition;

        BlockState state() const {
            return mLevel->getBlockState(mPosition.x, mPosition.y, mPosition.z);
        }
    };

    BlockAt blockAt(JSContext *ctx, JSValueConst thisVal) {
        BlockAt at;
        at.mApi = ScriptApi::fromContext(ctx);
        const ScriptBlockRef *ref = at.mApi->native<ScriptBlockRef>(thisVal);
        if (ref == nullptr) {
            if (const ScriptBlockPart *part = at.mApi->native<ScriptBlockPart>(thisVal))
                ref = &part->mBlock;
        }
        at.mLevel = &at.mApi->resolveLevel(ref->mWorldId, ref->mDimension);
        at.mPosition = Vector3i(ref->mX, ref->mY, ref->mZ);
        return at;
    }

    struct LiquidAnswers {
        bool mCanContain = false;
        bool mDestroyedBySpread = false;
        bool mSpawnsOnSpread = false;
        bool mBlocking = false;
        uint8_t mBlockedFaces = 0;
    };

    // LiquidType has Water alone: lava answers are the game's lava rules, not asked for here
    LiquidAnswers liquidAnswers(const BlockState &state) {
        const LiquidProperties properties = LiquidProperties::of(state);
        LiquidAnswers answers;
        answers.mCanContain = LiquidPhysicsSystem::getWaterloggingLevel(state) > 0;
        answers.mDestroyedBySpread = properties.reaction == LiquidReaction::Broken
                                     || properties.reaction == LiquidReaction::Popped;
        answers.mSpawnsOnSpread = properties.reaction == LiquidReaction::Popped;
        answers.mBlocking = properties.reaction == LiquidReaction::Blocking;
        answers.mBlockedFaces = properties.blockedFaces;
        return answers;
    }

    bool readWater(JSContext *ctx, JSValueConst value) {
        return JS_IsString(value) && toString(ctx, value) == "Water";
    }

    std::string normalized(std::string identifier) {
        if (identifier.find(':') == std::string::npos)
            identifier = "minecraft:" + identifier;
        return identifier;
    }

    // BlockType vtable +784 (sub_1435AC4B0): mDescriptionId + ".name", "tile." + the name without a minecraft namespace
    // (sub_1435A8150); a display_name component replaces it (sub_143169930)
    std::string localizationKeyOf(const std::string &identifier) {
        const CustomBlockDefinition *custom = CustomContentRegistry::getInstance().findBlock(identifier);
        if (custom != nullptr && !custom->mDisplayName.empty())
            return custom->mDisplayName;
        const bool vanilla = identifier.compare(0, 10, "minecraft:") == 0;
        const std::string name = vanilla ? identifier.substr(10) : identifier;
        if (!vanilla)
            return "tile." + name + ".name";

        // sub_14A433AC0
        static const std::unordered_map<std::string, std::string> fences = {
                {"oak_fence", "fence"},          {"acacia_fence", "acaciaFence"},
                {"birch_fence", "birchFence"},   {"dark_oak_fence", "darkOakFence"},
                {"jungle_fence", "jungleFence"}, {"spruce_fence", "spruceFence"}};
        // sub_1476FEEB0 over qword_14F319098
        static const std::unordered_map<std::string, std::string> planks = {
                {"oak_planks", "oak"},       {"spruce_planks", "spruce"}, {"birch_planks", "birch"},
                {"jungle_planks", "jungle"}, {"acacia_planks", "acacia"}, {"dark_oak_planks", "big_oak"}};
        // sub_14A429250 over off_14F3E5E38: element 1 to 118, any other number reads unknown
        static const char *const elements[] = {
                "h",  "he", "li", "be", "b",  "c",  "n",  "o",  "f",  "ne", "na", "mg", "al", "si", "p",  "s",  "cl",
                "ar", "k",  "ca", "sc", "ti", "v",  "cr", "mn", "fe", "co", "ni", "cu", "zn", "ga", "ge", "as", "se",
                "br", "kr", "rb", "sr", "y",  "zr", "nb", "mo", "tc", "ru", "rh", "pd", "ag", "cd", "in", "sn", "sb",
                "te", "i",  "xe", "cs", "ba", "la", "ce", "pr", "nd", "pm", "sm", "eu", "gd", "tb", "dy", "ho", "er",
                "tm", "yb", "lu", "hf", "ta", "w",  "re", "os", "ir", "pt", "au", "hg", "tl", "pb", "bi", "po", "at",
                "rn", "fr", "ra", "ac", "th", "pa", "u",  "np", "pu", "am", "cm", "bk", "cf", "es", "fm", "md", "no",
                "lr", "rf", "db", "sg", "bh", "hs", "mt", "ds", "rg", "cn", "nh", "fl", "mc", "lv", "ts", "og"};

        const auto fence = fences.find(name);
        if (fence != fences.end())
            return "tile." + fence->second + ".name";
        const auto plank = planks.find(name);
        if (plank != planks.end())
            return "tile.planks." + plank->second + ".name";
        // sub_148F8B380: the skull class swaps tile. for item.
        if (BlockIdentifier::equalsAny(identifier,
                                       {"minecraft:skeleton_skull", "minecraft:wither_skeleton_skull",
                                        "minecraft:zombie_head", "minecraft:player_head", "minecraft:creeper_head",
                                        "minecraft:dragon_head", "minecraft:piglin_head"}))
            return "item." + name + ".name";
        if (name == "brewing_stand")
            return "item.brewing_stand.name";
        if (name == "seagrass")
            return "tile.seagrass.seagrass.name";
        if (name.compare(0, 8, "element_") == 0 && name.size() > 8
            && name.find_first_not_of("0123456789", 8) == std::string::npos && name.size() <= 11) {
            const int number = std::stoi(name.substr(8));
            return std::string("tile.element.") + (number >= 1 && number <= 118 ? elements[number - 1] : "unknown")
                   + ".name";
        }
        return "tile." + name + ".name";
    }

    BlockState stateFromJs(JSContext *ctx, JSValueConst id, JSValueConst states) {
        const std::string identifier = normalized(toString(ctx, id));
        BlockState state = ScriptApi::defaultBlockState(identifier);
        const Tag read = statesFromJs(ctx, states);
        for (size_t index = 0; index < read.getKeys().size(); ++index)
            state.mStates.put(read.getKeys()[index], read.getValues()[index]);
        return BlockState(state.mName, state.mStates);
    }

    JSValue dataBlockInfo(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_UNDEFINED;
        const BlockState state = stateFromJs(ctx, argv[0], argc >= 2 ? argv[1] : JS_UNDEFINED);
        const LiquidAnswers liquid = liquidAnswers(state);

        JSValue info = JS_NewObject(ctx);
        JSValue tags = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::string &tag: BlockTags::all(state))
            JS_SetPropertyUint32(ctx, tags, index++, string(ctx, tag));
        JS_SetPropertyStr(ctx, info, "tags", tags);
        JS_SetPropertyStr(ctx, info, "localizationKey", string(ctx, localizationKeyOf(state.mName)));
        JS_SetPropertyStr(ctx, info, "canContainLiquid", JS_NewBool(ctx, liquid.mCanContain));
        JS_SetPropertyStr(ctx, info, "destroyedBySpread", JS_NewBool(ctx, liquid.mDestroyedBySpread));
        JS_SetPropertyStr(ctx, info, "spawnsOnSpread", JS_NewBool(ctx, liquid.mSpawnsOnSpread));
        JS_SetPropertyStr(ctx, info, "liquidBlocking", JS_NewBool(ctx, liquid.mBlocking));
        return info;
    }

    JSValue itemOfBlock(ScriptApi &api, const BlockState &state, int32_t amount) {
        const std::string identifier = BlockPickItem::identifierFor(state.mName);
        if (identifier.empty() || identifier == BlockNames::AIR)
            return JS_UNDEFINED;
        ScriptItemData data;
        data.mTypeId = identifier;
        data.mAmount = amount;
        ItemStack stack;
        if (!api.toItemStack(data, stack))
            return JS_UNDEFINED;
        return api.makeItemStack(identifier, amount);
    }

    int32_t readAmount(JSContext *ctx, int argc, JSValueConst *argv, int index) {
        int32_t amount = 1;
        if (argc > index && !JS_IsUndefined(argv[index]))
            JS_ToInt32(ctx, &amount, argv[index]);
        return amount;
    }

    JSValue dataPermutationItem(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc < 1)
            return JS_UNDEFINED;
        const int32_t amount = readAmount(ctx, argc, argv, 2);
        if (amount < 1 || amount > 255)
            return ScriptApi::fromContext(ctx)->throwError("ArgumentOutOfBoundsError", "");
        return itemOfBlock(*ScriptApi::fromContext(ctx), stateFromJs(ctx, argv[0], argv[1]), amount);
    }

    JSValue blockGetPermutation(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        return at.mApi->makePermutation(at.state());
    }

    JSValue blockGetType(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        JSValue permutation = blockGetPermutation(ctx, thisVal, 0, nullptr);
        JSValue type = JS_IsObject(permutation) ? JS_GetPropertyStr(ctx, permutation, "type") : JS_UNDEFINED;
        JS_FreeValue(ctx, permutation);
        return type;
    }

    // the permutation answers: one rule for both classes
    JSValue blockForward(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, const char *member) {
        JSValue permutation = blockGetPermutation(ctx, thisVal, 0, nullptr);
        if (!JS_IsObject(permutation))
            return JS_UNDEFINED;
        JSAtom atom = JS_NewAtom(ctx, member);
        JSValue result = JS_Invoke(ctx, permutation, atom, argc, argv);
        JS_FreeAtom(ctx, atom);
        JS_FreeValue(ctx, permutation);
        return result;
    }

    JSValue blockMatches(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        return blockForward(ctx, thisVal, argc, argv, "matches");
    }

    JSValue blockGetTags(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        return blockForward(ctx, thisVal, argc, argv, "getTags");
    }

    JSValue blockHasTag(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        return blockForward(ctx, thisVal, argc, argv, "hasTag");
    }

    JSValue blockGetLocalizationKey(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return string(ctx, localizationKeyOf(blockAt(ctx, thisVal).state().mName));
    }

    JSValue blockGetIsLiquid(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewBool(ctx, LiquidProperties::of(blockAt(ctx, thisVal).state()).isLiquid());
    }

    JSValue blockGetIsWaterlogged(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        if (LiquidProperties::of(at.state()).isLiquid())
            return JS_FALSE;
        const BlockState overlay = at.mLevel->getBlockStateAtLayer(at.mPosition.x, at.mPosition.y, at.mPosition.z, 1);
        return JS_NewBool(ctx, LiquidProperties::of(overlay).material == LiquidMaterial::Water);
    }

    JSValue blockSetWaterlogged(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const BlockAt at = blockAt(ctx, thisVal);
        if (argc < 1 || !JS_IsBool(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const bool waterlogged = JS_ToBool(ctx, argv[0]) == 1;
        // sub_14A754340: only setting true checks the block type
        if (waterlogged && !liquidAnswers(at.state()).mCanContain)
            return at.mApi->throwError("Error", "Block type cannot be waterlogged.");

        BlockState overlay = BlockState(BlockNames::AIR);
        if (waterlogged) {
            overlay = BlockState(BlockNames::WATER);
            overlay.mStates.putInt("liquid_depth", 0);
        }
        at.mLevel->setBlockStateAtLayer(at.mPosition.x, at.mPosition.y, at.mPosition.z, 1, overlay);
        BlockActionHandler::broadcastBlockUpdate(at.mApi->host(), *at.mLevel, at.mPosition, overlay, 1);
        return JS_UNDEFINED;
    }

    JSValue blockLiquidQuery(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        if (argc < 1 || !readWater(ctx, argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const LiquidAnswers answers = liquidAnswers(blockAt(ctx, thisVal).state());
        switch (magic) {
            case 0:
                return JS_NewBool(ctx, answers.mCanContain);
            case 1:
                return JS_NewBool(ctx, answers.mDestroyedBySpread);
            case 2:
                return JS_NewBool(ctx, answers.mSpawnsOnSpread);
            default:
                return JS_NewBool(ctx, answers.mBlocking);
        }
    }

    JSValue blockLiquidCanFlowFrom(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 2 || !readWater(ctx, argv[0]) || !JS_IsString(argv[1]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const int face = faceOf(toString(ctx, argv[1]));
        if (face < 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const LiquidAnswers answers = liquidAnswers(blockAt(ctx, thisVal).state());
        return JS_NewBool(ctx, !answers.mBlocking && ((answers.mBlockedFaces >> face) & 1) == 0);
    }

    JSValue blockBottomCenter(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        return vector3(ctx, (float) at.mPosition.x + 0.5f, (float) at.mPosition.y, (float) at.mPosition.z + 0.5f);
    }

    JSValue blockOffsetBy(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const BlockAt at = blockAt(ctx, thisVal);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 1 || !readVector3(ctx, argv[0], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const Vector3i target(at.mPosition.x + (int32_t) std::floor(x), at.mPosition.y + (int32_t) std::floor(y),
                              at.mPosition.z + (int32_t) std::floor(z));
        if (target.y < at.mLevel->getMinY() || target.y > at.mLevel->getMaxY())
            return at.mApi->throwError("LocationOutOfWorldBoundariesError", "");
        if (at.mLevel->peekBlockPtr(target.x, target.y, target.z) == nullptr)
            return JS_UNDEFINED;
        return at.mApi->makeBlock(*at.mLevel, target.x, target.y, target.z);
    }

    // sub_1432D1500: a block filtering all light reads 0, else the brighter of block light and darkened sky light
    JSValue blockGetLightLevel(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        if (BlockLightProperties::lightFilter(BlockLightProperties::packed(at.state())) >= 15)
            return JS_NewInt32(ctx, 0);
        const int block = at.mLevel->getBlockLightAt(at.mPosition.x, at.mPosition.y, at.mPosition.z);
        const int sky = at.mLevel->getSkyLightAt(at.mPosition.x, at.mPosition.y, at.mPosition.z)
                        - at.mLevel->getSkyLightSubtracted();
        return JS_NewInt32(ctx, std::max(block, std::max(sky, 0)));
    }

    JSValue blockGetSkyLightLevel(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        return JS_NewInt32(ctx, at.mLevel->getSkyLightAt(at.mPosition.x, at.mPosition.y, at.mPosition.z));
    }

    JSValue blockGetRedstonePower(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        return JS_NewInt32(ctx,
                           RedstoneSystem::isBlockIndirectlyGettingPowered(at.mApi->host(), *at.mLevel, at.mPosition));
    }

    JSValue blockGetItemStack(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const BlockAt at = blockAt(ctx, thisVal);
        const int32_t amount = readAmount(ctx, argc, argv, 0);
        if (amount < 1 || amount > 255)
            return at.mApi->throwError("ArgumentOutOfBoundsError", "");
        return itemOfBlock(*at.mApi, at.state(), amount);
    }

    // doors and double plants: the other half sits above or below
    JSValue blockGetParts(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        const BlockState state = at.state();
        const Tag *upper = state.mStates.get("upper_block_bit");
        if (upper == nullptr)
            return JS_UNDEFINED;
        const bool isUpper = upper->getType() == Tag::Type::Byte && upper->asByte() != 0;
        const Vector3i other(at.mPosition.x, at.mPosition.y + (isUpper ? -1 : 1), at.mPosition.z);
        JSValue parts = JS_NewArray(ctx);
        const Vector3i lower = isUpper ? other : at.mPosition;
        const Vector3i top = isUpper ? at.mPosition : other;
        JS_SetPropertyUint32(ctx, parts, 0, at.mApi->makeBlock(*at.mLevel, lower.x, lower.y, lower.z));
        JS_SetPropertyUint32(ctx, parts, 1, at.mApi->makeBlock(*at.mLevel, top.x, top.y, top.z));
        return parts;
    }

    // components each block has, in BlockComponentTypes order
    std::vector<std::string> componentIdsAt(Level &level, const Vector3i &position) {
        std::vector<std::string> ids;
        const BlockState state = level.getBlockState(position.x, position.y, position.z);
        BlockActor *actor = level.getBlockActors().find(position);
        if (actor != nullptr)
            ids.emplace_back("minecraft:dynamic_properties");
        if (CauldronBlock::matches(state.mName))
            ids.emplace_back("minecraft:fluid_container");
        const CustomBlockDefinition *custom = CustomContentRegistry::getInstance().findBlock(state.mName);
        if (custom != nullptr && custom->mInstrumentSound)
            ids.emplace_back("minecraft:instrument_sound");
        if (actor != nullptr && actor->getContainer() != nullptr && dynamic_cast<JukeboxBlockActor *>(actor) == nullptr)
            ids.emplace_back("minecraft:inventory");
        if (MapColors::find(state.mName) != nullptr)
            ids.emplace_back("minecraft:map_color");
        ids.emplace_back("minecraft:movable");
        if (PistonSystem::isPiston(state.mName))
            ids.emplace_back("minecraft:piston");
        ids.emplace_back("minecraft:precipitation_interactions");
        if (dynamic_cast<JukeboxBlockActor *>(actor) != nullptr)
            ids.emplace_back("minecraft:record_player");
        if (RedstoneSystem::isPowerSource(state))
            ids.emplace_back("minecraft:redstone_producer");
        if (dynamic_cast<SignBlockActor *>(actor) != nullptr)
            ids.emplace_back("minecraft:sign");
        return ids;
    }

    const char *const COMPONENT_CLASSES[][2] = {
            {"minecraft:dynamic_properties", "BlockDynamicPropertiesComponent"},
            {"minecraft:fluid_container", "BlockFluidContainerComponent"},
            {"minecraft:instrument_sound", "BlockInstrumentComponent"},
            {"minecraft:inventory", "BlockInventoryComponent"},
            {"minecraft:map_color", "BlockMapColorComponent"},
            {"minecraft:movable", "BlockMovableComponent"},
            {"minecraft:piston", "BlockPistonComponent"},
            {"minecraft:precipitation_interactions", "BlockPrecipitationInteractionsComponent"},
            {"minecraft:record_player", "BlockRecordPlayerComponent"},
            {"minecraft:redstone_producer", "BlockRedstoneProducerComponent"},
            {"minecraft:sign", "BlockSignComponent"},
    };

    const char *componentClassOf(const std::string &id) {
        for (const auto &entry: COMPONENT_CLASSES) {
            if (id == entry[0])
                return entry[1];
        }
        return nullptr;
    }

    JSValue blockGetComponent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const BlockAt at = blockAt(ctx, thisVal);
        if (argc < 1 || !JS_IsString(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return at.mApi->makeBlockComponent(*at.mLevel, at.mPosition, normalized(toString(ctx, argv[0])));
    }

    JSValue blockHasComponent(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        JSValue component = blockGetComponent(ctx, thisVal, argc, argv);
        if (JS_IsException(component))
            return component;
        const bool found = !JS_IsUndefined(component);
        JS_FreeValue(ctx, component);
        return JS_NewBool(ctx, found);
    }

    JSValue blockGetComponents(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        std::vector<std::string> ids = componentIdsAt(*at.mLevel, at.mPosition);
        const BlockState state = at.mLevel->getBlockState(at.mPosition.x, at.mPosition.y, at.mPosition.z);
        const CustomBlockDefinition *custom = CustomContentRegistry::getInstance().findBlock(state.mName);
        if (custom != nullptr) {
            for (const CustomComponentUse &use: CustomContentRegistry::componentsFor(*custom, state.mStates))
                ids.push_back(use.mName);
        }
        for (const std::string &id: ids) {
            JSValue component = at.mApi->makeBlockComponent(*at.mLevel, at.mPosition, id);
            if (JS_IsUndefined(component))
                continue;
            JS_SetPropertyUint32(ctx, array, index++, component);
        }
        return array;
    }

    const ScriptBlockPart &partOf(JSContext *ctx, JSValueConst thisVal) {
        return *ScriptApi::fromContext(ctx)->native<ScriptBlockPart>(thisVal);
    }

    JSValue blockCustomGetParameters(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        const std::optional<std::string> params = at.mApi->customBlockComponent(*at.mLevel, at.mPosition,
                                                                                partOf(ctx, thisVal).mComponentId);
        return params.has_value() ? at.mApi->makeComponentParameters(*params) : JS_UNDEFINED;
    }

    JSValue componentGetBlock(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        return at.mApi->makeBlock(*at.mLevel, at.mPosition.x, at.mPosition.y, at.mPosition.z);
    }

    JSValue inventoryGetContainer(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        return at.mApi->makeBlockContainer(*at.mLevel, at.mPosition);
    }

    // signs: the block actor's own tag is read and written back whole
    SignBlockActor *signAt(const BlockAt &at) {
        return at.mLevel->getBlockActors().find<SignBlockActor>(at.mPosition);
    }

    const char *signSide(JSContext *ctx, int argc, JSValueConst *argv, int index, bool &valid) {
        valid = true;
        if (argc <= index || JS_IsUndefined(argv[index]))
            return "FrontText";
        const std::string side = toString(ctx, argv[index]);
        if (side == "Front")
            return "FrontText";
        if (side == "Back")
            return "BackText";
        valid = false;
        return "FrontText";
    }

    JSValue signGetText(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        const BlockAt at = blockAt(ctx, thisVal);
        SignBlockActor *sign = signAt(at);
        bool valid = true;
        const char *side = signSide(ctx, argc, argv, 0, valid);
        if (!valid)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (sign == nullptr)
            return JS_UNDEFINED;
        const Tag saved = sign->saveNbt();
        const Tag *face = saved.get(side);
        const std::string text = face != nullptr && face->isCompound() ? face->getString("Text") : std::string();
        if (magic == 0)
            return string(ctx, text);
        JSValue parsed = text.empty() || text[0] != '{' ? JS_UNDEFINED
                                                        : JS_ParseJSON(ctx, text.c_str(), text.size(), "<sign>");
        if (JS_IsObject(parsed))
            return parsed;
        JS_FreeValue(ctx, parsed);
        JSValue raw = JS_NewObject(ctx);
        JSValue list = JS_NewArray(ctx);
        JSValue entry = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, entry, "text", string(ctx, text));
        JS_SetPropertyUint32(ctx, list, 0, entry);
        JS_SetPropertyStr(ctx, raw, "rawtext", list);
        return raw;
    }

    void writeSign(const BlockAt &at, SignBlockActor &sign, const Tag &data) {
        sign.loadNbt(data, at.mApi->host().getCodecContext());
        BlockActionHandler::broadcastBlockActorData(at.mApi->host(), *at.mLevel, sign);
    }

    JSValue signSetText(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const BlockAt at = blockAt(ctx, thisVal);
        SignBlockActor *sign = signAt(at);
        bool valid = true;
        const char *side = signSide(ctx, argc, argv, 1, valid);
        if (argc < 1 || !valid)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (sign == nullptr)
            return JS_UNDEFINED;
        bool isJson = false;
        const std::string text = JS_IsString(argv[0]) ? toString(ctx, argv[0]) : messageToRawText(ctx, argv[0], isJson);
        Tag data = sign->saveNbt();
        Tag face = data.get(side) != nullptr ? *data.get(side) : Tag::ofCompound();
        face.putString("Text", text);
        data.put(side, face);
        writeSign(at, *sign, data);
        return JS_UNDEFINED;
    }

    // ItemColor order with the dye texture rgb (qword_14F103E50); the sign keeps opaque rgb
    struct SignDye {
        const char *name;
        int32_t rgb;
    };
    const SignDye SIGN_DYES[] = {
            {"Black", 0x1D1D21},     {"Red", 0xB02E26},     {"Green", 0x5E7C16},  {"Brown", 0x835432},
            {"Blue", 0x3C44AA},      {"Purple", 0x8932B8},  {"Cyan", 0x169C9C},   {"Silver", 0x9D9D97},
            {"Gray", 0x474F52},      {"Pink", 0xF38BAA},    {"Lime", 0x80C71F},   {"Yellow", 0xFED83D},
            {"LightBlue", 0x3AB3DA}, {"Magenta", 0xC74EBD}, {"Orange", 0xF9801D}, {"White", 0xF0F0F0},
    };

    // only an exact dye rgb answers: the default black text (0) is undefined, sub_141A66520
    JSValue signGetTextDyeColor(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const BlockAt at = blockAt(ctx, thisVal);
        SignBlockActor *sign = signAt(at);
        bool valid = true;
        const char *side = signSide(ctx, argc, argv, 0, valid);
        if (!valid)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (sign == nullptr)
            return JS_UNDEFINED;
        const Tag saved = sign->saveNbt();
        const Tag *face = saved.get(side);
        const int32_t color = face != nullptr && face->isCompound() ? face->getInt("SignTextColor", -16777216)
                                                                    : -16777216;
        for (const SignDye &dye: SIGN_DYES) {
            if ((color & 0xFFFFFF) == dye.rgb)
                return JS_NewString(ctx, dye.name);
        }
        return JS_UNDEFINED;
    }

    JSValue signGetIsWaxed(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        SignBlockActor *sign = signAt(blockAt(ctx, thisVal));
        return JS_NewBool(ctx, sign != nullptr && sign->saveNbt().getBool("IsWaxed", false));
    }

    JSValue signSetWaxed(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const BlockAt at = blockAt(ctx, thisVal);
        SignBlockActor *sign = signAt(at);
        if (argc < 1 || !JS_IsBool(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (sign == nullptr)
            return JS_UNDEFINED;
        Tag data = sign->saveNbt();
        data.putBool("IsWaxed", JS_ToBool(ctx, argv[0]) == 1);
        writeSign(at, *sign, data);
        return JS_UNDEFINED;
    }

    PistonArmBlockActor *pistonAt(const BlockAt &at) {
        return at.mLevel->getBlockActors().find<PistonArmBlockActor>(at.mPosition);
    }

    JSValue pistonGetIsMoving(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const PistonArmBlockActor *piston = pistonAt(blockAt(ctx, thisVal));
        return JS_NewBool(ctx, piston != nullptr && piston->isMoving());
    }

    JSValue pistonGetState(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        const PistonArmBlockActor *piston = pistonAt(at);
        if (piston != nullptr && piston->isMoving())
            return JS_NewString(ctx, piston->isExtending() ? "Expanding" : "Retracting");
        return JS_NewString(ctx,
                            PistonSystem::isExtended(*at.mLevel, at.mPosition, at.state()) ? "Expanded" : "Retracted");
    }

    JSValue pistonGetAttached(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const BlockAt at = blockAt(ctx, thisVal);
        const PistonArmBlockActor *piston = pistonAt(at);
        JSValue array = JS_NewArray(ctx);
        if (piston == nullptr)
            return array;
        uint32_t index = 0;
        for (const Vector3i &position: piston->getAttachedBlocks()) {
            JSValue entry = magic == 0 ? at.mApi->makeBlock(*at.mLevel, position.x, position.y, position.z)
                                       : vector3(ctx, (float) position.x, (float) position.y, (float) position.z);
            JS_SetPropertyUint32(ctx, array, index++, entry);
        }
        return array;
    }

    JSValue fluidGetFillLevel(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewInt32(ctx, CauldronBlock(blockAt(ctx, thisVal).state()).getFillLevel());
    }

    void setCauldron(const BlockAt &at, CauldronBlock::Liquid liquid, int fillLevel) {
        at.mApi->setBlock(*at.mLevel, at.mPosition.x, at.mPosition.y, at.mPosition.z,
                          CauldronBlock::makeState(liquid, fillLevel));
    }

    JSValue fluidSetFillLevel(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const BlockAt at = blockAt(ctx, thisVal);
        int32_t level = 0;
        if (argc < 1 || !JS_IsNumber(argv[0]) || JS_ToInt32(ctx, &level, argv[0]) != 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        // fill_level spans 0 to 6
        if (level < 0 || level > 6)
            return at.mApi->throwError("ArgumentOutOfBoundsError", "");
        const CauldronBlock cauldron(at.state());
        const CauldronBlock::Liquid liquid = cauldron.getLiquid() == CauldronBlock::Liquid::Empty
                                                     ? CauldronBlock::Liquid::Water
                                                     : cauldron.getLiquid();
        setCauldron(at, level == 0 ? CauldronBlock::Liquid::Empty : liquid, level);
        return JS_UNDEFINED;
    }

    CauldronBlockActor *cauldronActorAt(const BlockAt &at) {
        return &at.mLevel->getBlockActors().getOrCreate<CauldronBlockActor>(at.mPosition);
    }

    // a potion cauldron keeps water as its liquid and a PotionId on its block actor
    JSValue fluidGetFluidType(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        const CauldronBlock cauldron(at.state());
        if (cauldron.getLiquid() == CauldronBlock::Liquid::Lava)
            return JS_NewString(ctx, "Lava");
        if (cauldron.getLiquid() == CauldronBlock::Liquid::PowderSnow)
            return JS_NewString(ctx, "PowderSnow");
        CauldronBlockActor *actor = at.mLevel->getBlockActors().find<CauldronBlockActor>(at.mPosition);
        if (actor != nullptr && actor->saveNbt().getShort("PotionId", -1) >= 0)
            return JS_NewString(ctx, "Potion");
        return JS_NewString(ctx, "Water");
    }

    JSValue fluidSetFluidType(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const BlockAt at = blockAt(ctx, thisVal);
        if (argc < 1 || !JS_IsString(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const std::string type = toString(ctx, argv[0]);
        if (type != "Water" && type != "Lava" && type != "PowderSnow")
            return at.mApi->throwError("Error", "Fluid type " + type + " is not supported.");
        const CauldronBlock cauldron(at.state());
        const int fill = std::max(cauldron.getFillLevel(), 1);
        setCauldron(at,
                    type == "Lava"         ? CauldronBlock::Liquid::Lava
                    : type == "PowderSnow" ? CauldronBlock::Liquid::PowderSnow
                                           : CauldronBlock::Liquid::Water,
                    fill);
        if (CauldronBlockActor *actor = at.mLevel->getBlockActors().find<CauldronBlockActor>(at.mPosition)) {
            Tag data = actor->saveNbt();
            data.putShort("PotionId", -1);
            data.putShort("PotionType", -1);
            actor->loadNbt(data, at.mApi->host().getCodecContext());
            BlockActionHandler::broadcastBlockActorData(at.mApi->host(), *at.mLevel, *actor);
        }
        return JS_UNDEFINED;
    }

    float channel(JSContext *ctx, JSValueConst color, const char *key, bool &valid) {
        JSValue value = JS_GetPropertyStr(ctx, color, key);
        double number = 0.0;
        valid = valid && JS_ToFloat64(ctx, &number, value) == 0 && std::isfinite(number);
        JS_FreeValue(ctx, value);
        return (float) std::clamp(number, 0.0, 1.0);
    }

    // CustomColor as the game saves it: an argb int
    JSValue fluidGetColor(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        CauldronBlockActor *actor = at.mLevel->getBlockActors().find<CauldronBlockActor>(at.mPosition);
        const Tag saved = actor != nullptr ? actor->saveNbt() : Tag::ofCompound();
        const uint32_t argb = saved.contains("CustomColor") ? (uint32_t) saved.getInt("CustomColor")
                                                            : MapBiomeColors().mWaterColor;
        const float color[4] = {(float) ((argb >> 16) & 0xff) / 255.0f, (float) ((argb >> 8) & 0xff) / 255.0f,
                                (float) (argb & 0xff) / 255.0f, (float) ((argb >> 24) & 0xff) / 255.0f};
        return rgba(ctx, color);
    }

    JSValue fluidSetColor(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const BlockAt at = blockAt(ctx, thisVal);
        if (argc < 1 || !JS_IsObject(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        bool valid = true;
        const float red = channel(ctx, argv[0], "red", valid), green = channel(ctx, argv[0], "green", valid),
                    blue = channel(ctx, argv[0], "blue", valid), alpha = channel(ctx, argv[0], "alpha", valid);
        if (!valid)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const auto byte = [](float value) { return (uint32_t) std::lround(value * 255.0f); };
        const uint32_t argb = byte(alpha) << 24 | byte(red) << 16 | byte(green) << 8 | byte(blue);
        CauldronBlockActor *actor = cauldronActorAt(at);
        Tag data = actor->saveNbt();
        data.putInt("CustomColor", (int32_t) argb);
        actor->loadNbt(data, at.mApi->host().getCodecContext());
        BlockActionHandler::broadcastBlockActorData(at.mApi->host(), *at.mLevel, *actor);
        return JS_UNDEFINED;
    }

    JSValue fluidSetPotion(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const BlockAt at = blockAt(ctx, thisVal);
        const ScriptItemRef *item = argc >= 1 ? at.mApi->native<ScriptItemRef>(argv[0]) : nullptr;
        if (item == nullptr)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const std::string &type = item->mItem->mTypeId;
        int16_t potionType = -1;
        if (type == "minecraft:potion")
            potionType = 0;
        else if (type == "minecraft:splash_potion")
            potionType = 1;
        else if (type == "minecraft:lingering_potion")
            potionType = 2;
        if (potionType < 0)
            return at.mApi->throwError("Error", "Item " + type + " is not a potion.");
        const CauldronBlock cauldron(at.state());
        setCauldron(at, CauldronBlock::Liquid::Water, std::max(cauldron.getFillLevel(), 1));
        CauldronBlockActor *actor = cauldronActorAt(at);
        Tag data = actor->saveNbt();
        data.putShort("PotionId", (int16_t) item->mItem->mDamage);
        data.putShort("PotionType", potionType);
        actor->loadNbt(data, at.mApi->host().getCodecContext());
        BlockActionHandler::broadcastBlockActorData(at.mApi->host(), *at.mLevel, *actor);
        return JS_UNDEFINED;
    }

    // ScriptBlockFluidContainerComponent::addDye (sub_14C8D2390, 26.60): the dye as a one item stack in dye slot 2
    JSValue fluidAddDye(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const BlockAt at = blockAt(ctx, thisVal);
        JSValue id = argc >= 1 && JS_IsObject(argv[0]) ? JS_GetPropertyStr(ctx, argv[0], "id") : JS_UNDEFINED;
        const bool named = JS_IsString(id);
        const std::string item = named ? toString(ctx, id) : std::string();
        JS_FreeValue(ctx, id);
        if (!named)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        CauldronBlockActor *actor = cauldronActorAt(at);
        actor->addDye(CauldronBlockActor::dyeColorOf(item));
        BlockActionHandler::broadcastBlockActorData(at.mApi->host(), *at.mLevel, *actor);
        return JS_UNDEFINED;
    }

    JSValue redstoneGetPower(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        int power = 0;
        for (int face = 0; face < RedstoneFace::COUNT; ++face)
            power = std::max(power, RedstoneSystem::getWeakPower(at.mApi->host(), *at.mLevel, at.mPosition, face));
        return JS_NewInt32(ctx, power);
    }

    // the face's note sound, note.none when undeclared; only Down and Up have one (sub_14E0A7680, 26.60)
    JSValue instrumentOf(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, BlockAt &at,
                         std::string &sound) {
        at = blockAt(ctx, thisVal);
        const int face = argc >= 1 && JS_IsString(argv[0]) ? faceOf(toString(ctx, argv[0])) : -1;
        if (face < 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (at.mPosition.y < at.mLevel->getMinY() || at.mPosition.y > at.mLevel->getMaxY())
            return at.mApi->throwError("LocationOutOfWorldBoundariesError", "");
        if (at.mLevel->peekChunkPtr(at.mPosition.x >> 4, at.mPosition.z >> 4) == nullptr)
            return at.mApi->throwError("LocationInUnloadedChunkError", "");
        if (face > 1)
            return at.mApi->throwError("InvalidArgumentError", "");
        sound = "note.none";
        const CustomBlockDefinition *custom = CustomContentRegistry::getInstance().findBlock(at.state().mName);
        if (custom == nullptr)
            return JS_UNDEFINED;
        for (const auto &entry: custom->mInstrumentSounds) {
            if (entry.first == (face == 0 ? "down" : "up"))
                sound = entry.second;
        }
        return JS_UNDEFINED;
    }

    JSValue instrumentGetName(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        BlockAt at;
        std::string sound;
        JSValue refused = instrumentOf(ctx, thisVal, argc, argv, at, sound);
        return JS_IsException(refused) ? refused : string(ctx, sound);
    }

    // note.none is played too: the client has no such sound (sub_14E10C700, 26.60)
    JSValue instrumentPlaySound(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        BlockAt at;
        std::string sound;
        JSValue refused = instrumentOf(ctx, thisVal, argc, argv, at, sound);
        if (JS_IsException(refused))
            return refused;
        ScriptSoundOptions options;
        refused = at.mApi->readSoundOptions(argc >= 2 ? argv[1] : JS_UNDEFINED, options, true);
        if (JS_IsException(refused))
            return refused;
        const Vector3f center((float) at.mPosition.x + 0.5f, (float) at.mPosition.y + 0.5f,
                              (float) at.mPosition.z + 0.5f);
        JS_FreeValue(ctx, at.mApi->playSound(*at.mLevel, nullptr, sound, center, options));
        return JS_UNDEFINED;
    }

    JSValue rgba(JSContext *ctx, const float color[4]) {
        JSValue value = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, value, "red", JS_NewFloat64(ctx, color[0]));
        JS_SetPropertyStr(ctx, value, "green", JS_NewFloat64(ctx, color[1]));
        JS_SetPropertyStr(ctx, value, "blue", JS_NewFloat64(ctx, color[2]));
        JS_SetPropertyStr(ctx, value, "alpha", JS_NewFloat64(ctx, color[3]));
        return value;
    }

    JSValue mapColorGet(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const BlockState state = blockAt(ctx, thisVal).state();
        const MapColors::Entry *entry = MapColors::find(state.mName);
        if (magic == 2) {
            static const char *const TINTS[] = {"None",       "DefaultFoliage", "BirchFoliage", "EvergreenFoliage",
                                                "DryFoliage", "Grass",          "Water"};
            const size_t tint = entry == nullptr ? 0 : (size_t) entry->mTint;
            return JS_NewString(ctx, tint < 7 ? TINTS[tint] : "None");
        }
        float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        if (magic == 0 && entry != nullptr) {
            for (int channel = 0; channel < 4; ++channel)
                color[channel] = (float) entry->mColor[channel] / 255.0f;
        } else if (magic == 1) {
            MapColors::getColor(state, MapBiomeColors(), color);
        }
        return rgba(ctx, color);
    }

    JSValue movableGet(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const BlockState state = blockAt(ctx, thisVal).state();
        if (magic == 1)
            return JS_NewString(ctx, PistonSystem::canSticksBlock(state) ? "Same" : "None");
        if (!PistonSystem::canBePushed(state))
            return JS_NewString(ctx, "Immovable");
        if (PistonSystem::breaksWhenMoved(state))
            return JS_NewString(ctx, "Popped");
        return JS_NewString(ctx, PistonSystem::canBePulled(state) ? "PushPull" : "Push");
    }

    JSValue precipitationQuery(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const BlockState state = blockAt(ctx, thisVal).state();
        if (magic == 0)
            return JS_NewBool(ctx, BlockPrecipitation::isSnowloggable(state));
        return JS_NewBool(ctx, !BlockPrecipitation::letsRainThrough(state));
    }

    JukeboxBlockActor *jukeboxAt(const BlockAt &at);

    std::string recordSound(const ItemStack &record);

    void playAt(const BlockAt &at, JukeboxBlockActor &jukebox);

    void stopAt(const BlockAt &at);

    JukeboxBlockActor *jukeboxAt(const BlockAt &at) {
        return at.mLevel->getBlockActors().find<JukeboxBlockActor>(at.mPosition);
    }

    JSValue recordGet(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        JukeboxBlockActor *jukebox = jukeboxAt(at);
        if (jukebox == nullptr)
            return JS_UNDEFINED;
        return at.mApi->makeItem(jukebox->getContainer()->getContainerItem(0));
    }

    JSValue recordSet(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const BlockAt at = blockAt(ctx, thisVal);
        JukeboxBlockActor *jukebox = jukeboxAt(at);
        if (jukebox == nullptr)
            return JS_UNDEFINED;
        ItemStack stack = ItemStack::air();
        if (argc >= 1 && !JS_IsUndefined(argv[0])) {
            JSValue id = JS_IsObject(argv[0]) ? JS_GetPropertyStr(ctx, argv[0], "id") : JS_DupValue(ctx, argv[0]);
            ScriptItemData data;
            data.mTypeId = normalized(toString(ctx, id));
            JS_FreeValue(ctx, id);
            if (!at.mApi->toItemStack(data, stack))
                return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        }
        if (!stack.isAir() && recordSound(stack).empty())
            return at.mApi->throwError("Error", "Item is not a record.");
        stopAt(at);
        jukebox->getContainer()->setContainerItem(0, stack);
        BlockActionHandler::broadcastBlockActorData(at.mApi->host(), *at.mLevel, *jukebox);
        // the one-argument overload plays (sub_14E153BD0); the other stores !startPlaying (sub_14E14CF60)
        if (argc < 2 || JS_IsUndefined(argv[1]) || JS_ToBool(ctx, argv[1]) == 1)
            playAt(at, *jukebox);
        return JS_UNDEFINED;
    }

    JSValue recordEject(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        JukeboxBlockActor *jukebox = jukeboxAt(at);
        if (jukebox == nullptr)
            return JS_UNDEFINED;
        const ItemStack record = jukebox->getContainer()->getContainerItem(0);
        if (record.isAir() || record.mDefinition == nullptr)
            return JS_UNDEFINED;
        stopAt(at);
        jukebox->getContainer()->setContainerItem(0, ItemStack::air());
        // sub_143C803A0: up, north, south, east, west, down, the first free block takes the record
        static const int SIDES[6][3] = {{0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {1, 0, 0}, {-1, 0, 0}, {0, -1, 0}};
        Vector3i target(at.mPosition.x, at.mPosition.y + 1, at.mPosition.z);
        for (const int *side: SIDES) {
            const Vector3i candidate(at.mPosition.x + side[0], at.mPosition.y + side[1], at.mPosition.z + side[2]);
            if (at.mLevel->getBlockState(candidate.x, candidate.y, candidate.z).mName == BlockNames::AIR) {
                target = candidate;
                break;
            }
        }
        ItemActorHandler::dropItem(at.mApi->host(), *at.mLevel, ItemActorHandler::randomPopPosition(target), record,
                                   ItemActorHandler::randomDropMotion(), ItemActorHandler::DROP_PICKUP_DELAY);
        BlockActionHandler::broadcastBlockActorData(at.mApi->host(), *at.mLevel, *jukebox);
        return JS_UNDEFINED;
    }

    JSValue redstoneGetConnectedFaces(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (int face = 0; face < RedstoneFace::COUNT; ++face) {
            if (RedstoneSystem::getWeakPower(at.mApi->host(), *at.mLevel, at.mPosition, face) > 0)
                JS_SetPropertyUint32(ctx, array, index++, JS_NewString(ctx, DIRECTIONS[face]));
        }
        return array;
    }

    JSValue redstoneGetStronglyPoweredFace(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        for (int face = 0; face < RedstoneFace::COUNT; ++face) {
            if (RedstoneSystem::getStrongPower(at.mApi->host(), *at.mLevel, at.mPosition, face) > 0)
                return JS_NewString(ctx, DIRECTIONS[face]);
        }
        return JS_UNDEFINED;
    }

    JSValue precipitationAccumulatesSnow(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        return JS_NewBool(ctx, PrecipitationSystem::canHoldSnow(*at.mLevel, at.mPosition));
    }

    // the record sound events of the 26.60 strings, record.<disc>
    const char *const RECORDS[] = {"11",        "13",           "5",
                                   "blocks",    "bounce",       "cat",
                                   "chirp",     "creator",      "creator_music_box",
                                   "far",       "lava_chicken", "mall",
                                   "mellohi",   "otherside",    "pigstep",
                                   "precipice", "relic",        "stal",
                                   "strad",     "tears",        "wait",
                                   "ward"};

    std::string recordSound(const ItemStack &record) {
        if (record.isAir() || record.mDefinition == nullptr)
            return std::string();
        const std::string &identifier = record.mDefinition->getIdentifier();
        const std::string prefix = "minecraft:music_disc_";
        if (identifier.compare(0, prefix.size(), prefix) != 0)
            return std::string();
        const std::string disc = identifier.substr(prefix.size());
        for (const char *known: RECORDS) {
            if (disc == known)
                return "record." + disc;
        }
        return std::string();
    }

    bool isPlayingAt(const BlockAt &at) {
        const JukeboxBlockActor *jukebox = jukeboxAt(at);
        return jukebox != nullptr && jukebox->isPlaying();
    }

    Vector3f centerOf(const BlockAt &at) {
        return Vector3f((float) at.mPosition.x + 0.5f, (float) at.mPosition.y + 0.5f, (float) at.mPosition.z + 0.5f);
    }

    void playAt(const BlockAt &at, JukeboxBlockActor &jukebox) {
        const std::string sound = recordSound(jukebox.getContainer()->getContainerItem(0));
        if (sound.empty())
            return;
        at.mApi->host().playBlockSound(*at.mLevel, sound, centerOf(at), at.state().getHash());
        jukebox.setPlaying(true);
        BlockActionHandler::broadcastBlockActorData(at.mApi->host(), *at.mLevel, jukebox);
    }

    // no sound event: the game only raises FinishedRecording, the client stops the disc (sub_144C37130)
    void stopAt(const BlockAt &at) {
        JukeboxBlockActor *jukebox = jukeboxAt(at);
        if (jukebox == nullptr || !jukebox->isPlaying())
            return;
        jukebox->setPlaying(false);
        BlockActionHandler::broadcastBlockActorData(at.mApi->host(), *at.mLevel, *jukebox);
    }

    JSValue recordPlay(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const BlockAt at = blockAt(ctx, thisVal);
        if (JukeboxBlockActor *jukebox = jukeboxAt(at))
            playAt(at, *jukebox);
        return JS_UNDEFINED;
    }

    JSValue recordPause(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        stopAt(blockAt(ctx, thisVal));
        return JS_UNDEFINED;
    }

    JSValue recordIsPlaying(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewBool(ctx, isPlayingAt(blockAt(ctx, thisVal)));
    }

    // stored on the block actor and saved with it; a block without one has none
    std::unordered_map<std::string, DynamicPropertyValue> *propertiesAt(const BlockAt &at) {
        BlockActor *actor = at.mLevel->getBlockActors().find(at.mPosition);
        return actor == nullptr ? nullptr : &actor->getDynamicProperties();
    }

    JSValue dynamicGet(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        if (argc < 1 || !JS_IsString(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        const auto *properties = propertiesAt(blockAt(ctx, thisVal));
        if (properties == nullptr)
            return JS_UNDEFINED;
        return dynamicPropertyGet(ctx, *properties, toString(ctx, argv[0]));
    }

    // 1KB per block actor, from the definitions
    constexpr int64_t BLOCK_PROPERTY_LIMIT = 1024;

    JSValue dynamicSet(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const BlockAt at = blockAt(ctx, thisVal);
        if (argc < 1 || !JS_IsString(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        auto *properties = propertiesAt(at);
        if (properties == nullptr)
            return JS_UNDEFINED;
        const auto before = *properties;
        dynamicPropertySet(ctx, *properties, toString(ctx, argv[0]), argc >= 2 ? argv[1] : JS_UNDEFINED);
        if (JS_HasException(ctx))
            return JS_EXCEPTION;
        if (dynamicPropertyByteCount(*properties) > BLOCK_PROPERTY_LIMIT) {
            *properties = before;
            return at.mApi->throwError("Error", "Block dynamic properties exceed 1KB.");
        }
        return JS_UNDEFINED;
    }

    JSValue dynamicByteCount(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const auto *properties = propertiesAt(blockAt(ctx, thisVal));
        return JS_NewInt64(ctx, properties == nullptr ? 0 : dynamicPropertyByteCount(*properties));
    }

    // undefined writes opaque black, a dye its texture rgb with full alpha (sub_14C8C4BD0)
    JSValue signSetTextDyeColor(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const BlockAt at = blockAt(ctx, thisVal);
        SignBlockActor *sign = signAt(at);
        bool valid = true;
        const char *side = signSide(ctx, argc, argv, 1, valid);
        if (!valid)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        int32_t color = -16777216;
        if (argc >= 1 && !JS_IsUndefined(argv[0])) {
            const std::string name = toString(ctx, argv[0]);
            const auto dye = std::find_if(std::begin(SIGN_DYES), std::end(SIGN_DYES),
                                          [&name](const SignDye &entry) { return name == entry.name; });
            if (dye == std::end(SIGN_DYES))
                return JS_ThrowTypeError(ctx, "Native type conversion failed.");
            color = (int32_t) (0xFF000000u | (uint32_t) dye->rgb);
        }
        if (sign == nullptr)
            return JS_UNDEFINED;
        Tag data = sign->saveNbt();
        Tag face = data.get(side) != nullptr ? *data.get(side) : Tag::ofCompound();
        face.putInt("SignTextColor", color);
        data.put(side, face);
        writeSign(at, *sign, data);
        return JS_UNDEFINED;
    }
}

namespace ScriptBlockData {
    JSCFunction *blockInfo() {
        return dataBlockInfo;
    }

    JSCFunction *permutationItem() {
        return dataPermutationItem;
    }
}

std::optional<std::string> ScriptApi::customBlockComponent(Level &level, const Vector3i &position,
                                                           const std::string &id) {
    const BlockState state = level.getBlockState(position.x, position.y, position.z);
    const CustomBlockDefinition *custom = CustomContentRegistry::getInstance().findBlock(state.mName);
    if (custom == nullptr)
        return std::nullopt;
    for (const CustomComponentUse &use: CustomContentRegistry::componentsFor(*custom, state.mStates)) {
        if (use.mName == id)
            return use.mParams;
    }
    return std::nullopt;
}

JSValue ScriptApi::makeBlockComponent(Level &level, const Vector3i &position, const std::string &id) {
    const char *cls = componentClassOf(id);
    // any other id: the custom component the block binds where it stands (BlockComponentReturnType)
    if (cls == nullptr && customBlockComponent(level, position, id).has_value())
        cls = "BlockCustomComponentInstance";
    if (cls == nullptr || !mSurface.has(cls))
        return JS_UNDEFINED;
    const std::vector<std::string> ids = componentIdsAt(level, position);
    if (std::strcmp(cls, "BlockCustomComponentInstance") != 0 && std::find(ids.begin(), ids.end(), id) == ids.end())
        return JS_UNDEFINED;

    ScriptBlockPart *part = new ScriptBlockPart();
    part->mBlock.mWorldId = level.getWorldId();
    part->mBlock.mDimension = level.getDimensionType();
    part->mBlock.mX = position.x;
    part->mBlock.mY = position.y;
    part->mBlock.mZ = position.z;
    part->mComponentId = id;
    return makeInstance(cls, part);
}

void ScriptApi::_bindBlockSurface() {
    if (mSurface.has("BlockBoundingBoxUtils")) {
        defineClass("BlockBoundingBoxUtils");
        JSValue utils = constructorOf("BlockBoundingBoxUtils");
        method(utils, "BlockBoundingBoxUtils", "static createValid", boxCreateValid, 2);
        method(utils, "BlockBoundingBoxUtils", "static dilate", boxDilate, 2);
        method(utils, "BlockBoundingBoxUtils", "static equals", boxEquals, 2);
        method(utils, "BlockBoundingBoxUtils", "static expand", boxExpand, 2);
        method(utils, "BlockBoundingBoxUtils", "static getCenter", boxGetCenter, 1);
        method(utils, "BlockBoundingBoxUtils", "static getIntersection", boxGetIntersection, 2);
        method(utils, "BlockBoundingBoxUtils", "static getSpan", boxGetSpan, 1);
        method(utils, "BlockBoundingBoxUtils", "static intersects", boxIntersects, 2);
        method(utils, "BlockBoundingBoxUtils", "static isInside", boxIsInside, 2);
        method(utils, "BlockBoundingBoxUtils", "static isValid", boxIsValid, 1);
        method(utils, "BlockBoundingBoxUtils", "static translate", boxTranslate, 2);
    }

    JSValue block = prototypeOf("Block");
    property(block, "Block", "permutation", blockGetPermutation);
    property(block, "Block", "type", blockGetType);
    property(block, "Block", "localizationKey", blockGetLocalizationKey);
    property(block, "Block", "isLiquid", blockGetIsLiquid);
    property(block, "Block", "isWaterlogged", blockGetIsWaterlogged);
    method(block, "Block", "matches", blockMatches, 2);
    method(block, "Block", "getTags", blockGetTags, 0);
    method(block, "Block", "hasTag", blockHasTag, 1);
    method(block, "Block", "setWaterlogged", blockSetWaterlogged, 1);
    methodMagic(block, "Block", "canContainLiquid", blockLiquidQuery, 1, 0);
    methodMagic(block, "Block", "canBeDestroyedByLiquidSpread", blockLiquidQuery, 1, 1);
    methodMagic(block, "Block", "liquidSpreadCausesSpawn", blockLiquidQuery, 1, 2);
    methodMagic(block, "Block", "isLiquidBlocking", blockLiquidQuery, 1, 3);
    method(block, "Block", "liquidCanFlowFromDirection", blockLiquidCanFlowFrom, 2);
    method(block, "Block", "bottomCenter", blockBottomCenter, 0);
    method(block, "Block", "offset", blockOffsetBy, 1);
    method(block, "Block", "getLightLevel", blockGetLightLevel, 0);
    method(block, "Block", "getSkyLightLevel", blockGetSkyLightLevel, 0);
    method(block, "Block", "getRedstonePower", blockGetRedstonePower, 0);
    method(block, "Block", "getItemStack", blockGetItemStack, 2);
    method(block, "Block", "getParts", blockGetParts, 0);
    method(block, "Block", "getComponent", blockGetComponent, 1);
    method(block, "Block", "getComponents", blockGetComponents, 0);
    method(block, "Block", "hasComponent", blockHasComponent, 1);

    if (mSurface.has("BlockComponent")) {
        JSValue base = defineClass("BlockComponent");
        property(base, "BlockComponent", "block", componentGetBlock);
    }
    if (mSurface.has("BlockCustomComponentInstance")) {
        JSValue custom = defineClass("BlockCustomComponentInstance");
        property(custom, "BlockCustomComponentInstance", "customComponentParameters", blockCustomGetParameters);
    }
    if (mSurface.has("BlockInventoryComponent")) {
        JSValue inventory = defineClass("BlockInventoryComponent");
        property(inventory, "BlockInventoryComponent", "container", inventoryGetContainer);
    }
    if (mSurface.has("BlockSignComponent")) {
        JSValue sign = defineClass("BlockSignComponent");
        property(sign, "BlockSignComponent", "isWaxed", signGetIsWaxed);
        methodMagic(sign, "BlockSignComponent", "getText", signGetText, 1, 0);
        methodMagic(sign, "BlockSignComponent", "getRawText", signGetText, 1, 1);
        method(sign, "BlockSignComponent", "setText", signSetText, 2);
        method(sign, "BlockSignComponent", "getTextDyeColor", signGetTextDyeColor, 1);
        method(sign, "BlockSignComponent", "setWaxed", signSetWaxed, 1);
        method(sign, "BlockSignComponent", "setTextDyeColor", signSetTextDyeColor, 2);
    }
    if (mSurface.has("BlockPistonComponent")) {
        JSValue piston = defineClass("BlockPistonComponent");
        property(piston, "BlockPistonComponent", "isMoving", pistonGetIsMoving);
        property(piston, "BlockPistonComponent", "state", pistonGetState);
        methodMagic(piston, "BlockPistonComponent", "getAttachedBlocks", pistonGetAttached, 0, 0);
        methodMagic(piston, "BlockPistonComponent", "getAttachedBlocksLocations", pistonGetAttached, 0, 1);
    }
    if (mSurface.has("BlockFluidContainerComponent")) {
        JSValue fluid = defineClass("BlockFluidContainerComponent");
        property(fluid, "BlockFluidContainerComponent", "fillLevel", fluidGetFillLevel, fluidSetFillLevel);
        method(fluid, "BlockFluidContainerComponent", "getFluidType", fluidGetFluidType, 0);
        method(fluid, "BlockFluidContainerComponent", "setFluidType", fluidSetFluidType, 1);
        property(fluid, "BlockFluidContainerComponent", "fluidColor", fluidGetColor, fluidSetColor);
        method(fluid, "BlockFluidContainerComponent", "setPotion", fluidSetPotion, 1);
        method(fluid, "BlockFluidContainerComponent", "addDye", fluidAddDye, 1);
    }
    if (mSurface.has("BlockRedstoneProducerComponent")) {
        JSValue producer = defineClass("BlockRedstoneProducerComponent");
        property(producer, "BlockRedstoneProducerComponent", "power", redstoneGetPower);
        method(producer, "BlockRedstoneProducerComponent", "getConnectedFaces", redstoneGetConnectedFaces, 0);
        method(producer, "BlockRedstoneProducerComponent", "getStronglyPoweredFace", redstoneGetStronglyPoweredFace, 0);
    }
    if (mSurface.has("BlockInstrumentComponent")) {
        JSValue instrument = defineClass("BlockInstrumentComponent");
        method(instrument, "BlockInstrumentComponent", "getInstrumentName", instrumentGetName, 1);
        method(instrument, "BlockInstrumentComponent", "playInstrumentSound", instrumentPlaySound, 2);
    }
    if (mSurface.has("BlockMapColorComponent")) {
        JSValue color = defineClass("BlockMapColorComponent");
        propertyMagic(color, "BlockMapColorComponent", "color", mapColorGet, 0);
        propertyMagic(color, "BlockMapColorComponent", "tintedColor", mapColorGet, 1);
        propertyMagic(color, "BlockMapColorComponent", "tintMethod", mapColorGet, 2);
    }
    if (mSurface.has("BlockMovableComponent")) {
        JSValue movable = defineClass("BlockMovableComponent");
        propertyMagic(movable, "BlockMovableComponent", "movementType", movableGet, 0);
        propertyMagic(movable, "BlockMovableComponent", "stickyType", movableGet, 1);
    }
    if (mSurface.has("BlockPrecipitationInteractionsComponent")) {
        JSValue precipitation = defineClass("BlockPrecipitationInteractionsComponent");
        methodMagic(precipitation, "BlockPrecipitationInteractionsComponent", "isSnowLoggable", precipitationQuery, 0,
                    0);
        methodMagic(precipitation, "BlockPrecipitationInteractionsComponent", "obstructsRain", precipitationQuery, 0,
                    1);
        method(precipitation, "BlockPrecipitationInteractionsComponent", "accumulatesSnow",
               precipitationAccumulatesSnow, 0);
    }
    if (mSurface.has("BlockRecordPlayerComponent")) {
        JSValue record = defineClass("BlockRecordPlayerComponent");
        method(record, "BlockRecordPlayerComponent", "getRecord", recordGet, 0);
        method(record, "BlockRecordPlayerComponent", "setRecord", recordSet, 2);
        method(record, "BlockRecordPlayerComponent", "ejectRecord", recordEject, 0);
        method(record, "BlockRecordPlayerComponent", "playRecord", recordPlay, 0);
        method(record, "BlockRecordPlayerComponent", "pauseRecord", recordPause, 0);
        method(record, "BlockRecordPlayerComponent", "isPlaying", recordIsPlaying, 0);
    }
    if (mSurface.has("BlockDynamicPropertiesComponent")) {
        JSValue dynamic = defineClass("BlockDynamicPropertiesComponent");
        method(dynamic, "BlockDynamicPropertiesComponent", "get", dynamicGet, 1);
        method(dynamic, "BlockDynamicPropertiesComponent", "set", dynamicSet, 2);
        method(dynamic, "BlockDynamicPropertiesComponent", "totalByteCount", dynamicByteCount, 0);
    }

    // the 1.x typings declare componentId before BlockComponentTypes lists the component
    for (const auto &entry: COMPONENT_CLASSES) {
        JSValue constructor = constructorOf(entry[1]);
        if (!JS_IsUndefined(constructor) && mSurface.has(std::string(entry[1]) + ".static componentId"))
            JS_DefinePropertyValueStr(mContext, constructor, "componentId", string(mContext, entry[0]),
                                      JS_PROP_ENUMERABLE);
    }
}
