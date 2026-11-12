#include "Scripting/Binding/ScriptApi.h"
#include <functional>

#include "World/Actor/ServerActor.h"
#include "World/Actor/Misc/ItemActor.h"
#include "Network/Handler/ItemActorHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Level/GameRules.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/PlaySoundPacket.h"
#include "Scripting/ScriptEngine.h"
#include "World/Level/Block/BlockNames.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#else
#include <unistd.h>
#endif

using namespace ScriptValues;

namespace {
    JSValue systemRun(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        JSValue callback = JS_DupValue(ctx, argv[0]);
        void *pointer = JS_VALUE_GET_PTR(callback);

        int64_t ticks = 1;
        if (magic != 0 && argc >= 2)
            JS_ToInt64(ctx, &ticks, argv[1]);

        uint32_t id;
        if (magic == 0)
            id = api->scheduler().run(pointer);
        else if (magic == 1)
            id = api->scheduler().runTimeout(pointer, ticks);
        else
            id = api->scheduler().runInterval(pointer, ticks);
        return JS_NewUint32(ctx, id);
    }

    JSValue systemClearRun(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        uint32_t id = 0;
        if (argc >= 1 && JS_ToUint32(ctx, &id, argv[0]) == 0)
            api->scheduler().clear(id);
        return JS_UNDEFINED;
    }

    JSValue systemGetCurrentTick(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_NewInt64(ctx, ScriptApi::fromContext(ctx)->host().getCurrentTick());
    }

    JSValue holderGetter(JSContext *ctx, JSValueConst, int, JSValueConst *, int magic) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        switch (magic) {
            case 0:
                return JS_DupValue(ctx, api->worldAfterEvents());
            case 1:
                return JS_DupValue(ctx, api->worldBeforeEvents());
            case 2:
                return JS_DupValue(ctx, api->systemAfterEvents());
            case 3:
                return JS_DupValue(ctx, api->systemBeforeEvents());
            case 4:
                return JS_DupValue(ctx, api->moduleScoreboard());
            case 6:
                return JS_DupValue(ctx, api->gameRules());
            case 7:
                return JS_DupValue(ctx, api->serverSystemInfo());
            default:
                return JS_DupValue(ctx, api->moduleTickingAreas());
        }
    }

    JSValue systemGetIsEditorWorld(JSContext *, JSValueConst, int, JSValueConst *) {
        return JS_FALSE;
    }

    // MemoryTier by the ceilings the typings give: 1.5, 2, 4 and 8 GiB, above is SuperHigh
    int32_t memoryTierOf(uint64_t bytes) {
        const uint64_t gib = 1024ull * 1024ull * 1024ull;
        if (bytes <= gib * 3 / 2)
            return 0;
        if (bytes <= gib * 2)
            return 1;
        if (bytes <= gib * 4)
            return 2;
        if (bytes <= gib * 8)
            return 3;
        return 4;
    }

    uint64_t physicalMemory() {
#if defined(_WIN32)
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        return GlobalMemoryStatusEx(&status) ? (uint64_t) status.ullTotalPhys : 0;
#elif defined(__APPLE__)
        uint64_t bytes = 0;
        size_t size = sizeof(bytes);
        return sysctlbyname("hw.memsize", &bytes, &size, nullptr, 0) == 0 ? bytes : 0;
#else
        const long pages = sysconf(_SC_PHYS_PAGES);
        const long pageSize = sysconf(_SC_PAGE_SIZE);
        return pages > 0 && pageSize > 0 ? (uint64_t) pages * (uint64_t) pageSize : 0;
#endif
    }

    // the server's own tier, or the client's on a ClientSystemInfo
    JSValue systemInfoGetMemoryTier(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (const ScriptPlayerPart *part = api->native<ScriptPlayerPart>(thisVal)) {
            ServerPlayer *player = api->resolvePlayerByHandle(part->mHandle);
            if (player == nullptr)
                return api->throwError("InvalidEntityError", "The player has left.");
            return JS_NewInt32(ctx, player->getClientDevice().mMemoryTier);
        }
        static const int32_t tier = memoryTierOf(physicalMemory());
        return JS_NewInt32(ctx, tier);
    }

    JSValue systemWaitTicks(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        double ticks = 0.0;
        if (argc < 1 || !JS_IsNumber(argv[0]) || JS_ToFloat64(ctx, &ticks, argv[0]) != 0)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (!std::isfinite(ticks) || ticks < 1.0)
            return api->throwError("EngineError", "");

        JSValue resolvingFuncs[2];
        JSValue promise = JS_NewPromiseCapability(ctx, resolvingFuncs);
        JS_FreeValue(ctx, resolvingFuncs[1]);
        // the scheduler owns the resolver and calls it with no argument
        api->scheduler().runTimeout(JS_VALUE_GET_PTR(resolvingFuncs[0]), (int64_t) ticks);
        return promise;
    }

    JSValue systemSendScriptEvent(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        const std::string id = toString(ctx, argv[0]);
        const size_t colon = id.find(':');
        const char *reason = nullptr;
        if (colon == std::string::npos || colon == 0 || colon + 1 >= id.size())
            reason = "NoNamespace";
        else if (id.compare(0, colon, "minecraft") == 0)
            reason = "DisallowedNamespace";
        if (reason != nullptr) {
            JSValue fields = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, fields, "reason", JS_NewString(ctx, reason));
            return api->throwError("NamespaceNameError", "", fields);
        }

        api->host().getScriptEngine().queueScriptEvent(id, toString(ctx, argv[1]));
        return JS_UNDEFINED;
    }

    World &defaultWorld(JSContext *ctx) {
        return ScriptApi::fromContext(ctx)->host().getWorlds().getDefault();
    }

    // official names; the server keys its rules in lowercase
    const char *const GAME_RULES[] = {"commandBlockOutput",
                                      "commandBlocksEnabled",
                                      "doDayLightCycle",
                                      "doEntityDrops",
                                      "doFireTick",
                                      "doImmediateRespawn",
                                      "doInsomnia",
                                      "doLimitedCrafting",
                                      "doMobLoot",
                                      "doMobSpawning",
                                      "doTileDrops",
                                      "doWeatherCycle",
                                      "drowningDamage",
                                      "fallDamage",
                                      "fireDamage",
                                      "freezeDamage",
                                      "functionCommandLimit",
                                      "keepInventory",
                                      "maxCommandChainLength",
                                      "mobGriefing",
                                      "naturalRegeneration",
                                      "playersSleepingPercentage",
                                      "projectilesCanBreakBlocks",
                                      "pvp",
                                      "randomTickSpeed",
                                      "recipesUnlock",
                                      "respawnBlocksExplode",
                                      "sendCommandFeedback",
                                      "showBorderEffect",
                                      "showCoordinates",
                                      "showDaysPlayed",
                                      "showDeathMessages",
                                      "showRecipeMessages",
                                      "showTags",
                                      "spawnRadius",
                                      "tntExplodes",
                                      "tntExplosionDropDecay"};

    std::string gameRuleKey(int magic) {
        std::string key = GAME_RULES[magic];
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char character) { return (char) std::tolower(character); });
        return key;
    }

    JSValue gameRulesGet(JSContext *ctx, JSValueConst, int, JSValueConst *, int magic) {
        const GameRules::Rule *rule = defaultWorld(ctx).getOverworld().getGameRules().find(gameRuleKey(magic));
        if (rule == nullptr)
            return JS_UNDEFINED;
        if (rule->mType == GameRuleData::Type::Bool)
            return JS_NewBool(ctx, rule->mBoolValue);
        return JS_NewInt32(ctx, rule->mIntValue);
    }

    JSValue gameRulesSet(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic) {
        World &world = defaultWorld(ctx);
        const std::string key = gameRuleKey(magic);
        const GameRules::Rule *rule = world.getOverworld().getGameRules().find(key);
        if (rule == nullptr || argc < 1)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        std::string value;
        if (rule->mType == GameRuleData::Type::Bool) {
            if (!JS_IsBool(argv[0]))
                return JS_ThrowTypeError(ctx, "Native type conversion failed.");
            value = JS_ToBool(ctx, argv[0]) == 1 ? "true" : "false";
        } else {
            int32_t number = 0;
            if (!JS_IsNumber(argv[0]) || JS_ToInt32(ctx, &number, argv[0]) != 0)
                return JS_ThrowTypeError(ctx, "Native type conversion failed.");
            value = std::to_string(number);
        }
        ScriptApi::fromContext(ctx)->host().changeGameRule(world, key, value);
        return JS_UNDEFINED;
    }

    JSValue worldGetIsHardcore(JSContext *, JSValueConst, int, JSValueConst *) {
        return JS_FALSE;
    }

    JSValue worldGetSeed(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return string(ctx, std::to_string(defaultWorld(ctx).getSeed()));
    }

    JSValue worldGetTime(JSContext *ctx, JSValueConst, int, JSValueConst *, int magic) {
        const Level &level = defaultWorld(ctx).getOverworld();
        switch (magic) {
            case 0:
                return JS_NewInt64(ctx, level.getTime());
            case 1:
                return JS_NewInt64(ctx, level.getDayTime());
            case 2:
                return JS_NewInt64(ctx, level.getTime() / Level::TICKS_PER_DAY);
            default:
                // MoonPhase counts from the full moon, one phase a day
                return JS_NewInt32(ctx, (int32_t) ((level.getTime() / Level::TICKS_PER_DAY) % 8));
        }
    }

    JSValue worldSetTime(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv, int magic) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        double time = 0.0;
        if (argc < 1 || !JS_IsNumber(argv[0]) || JS_ToFloat64(ctx, &time, argv[0]) != 0 || !std::isfinite(time))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        World &world = defaultWorld(ctx);
        Level &level = world.getOverworld();
        if (magic == 0) {
            level.setTime((int64_t) time);
        } else {
            if (time < 0.0 || time > (double) Level::TICKS_PER_DAY)
                return api->throwError("Error", "");
            // keeps the day count: only the time of day moves
            level.setTime(level.getTime() / Level::TICKS_PER_DAY * Level::TICKS_PER_DAY + (int64_t) time);
        }
        api->host().broadcastWorldTime(world);
        return JS_UNDEFINED;
    }

    JSValue worldGetDefaultSpawnLocation(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        const Vector3i spawn = defaultWorld(ctx).getOverworld().getSpawnPosition();
        return vector3(ctx, (float) spawn.x, (float) spawn.y, (float) spawn.z);
    }

    JSValue worldSetDefaultSpawnLocation(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 1 || !readVector3(ctx, argv[0], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        Level &level = defaultWorld(ctx).getOverworld();
        const Vector3i spawn((int32_t) std::floor(x), (int32_t) std::floor(y), (int32_t) std::floor(z));
        if (spawn.y < level.getMinY() || spawn.y > level.getMaxY())
            return api->throwError("LocationOutOfWorldBoundariesError", "");
        api->host().changeWorldSpawn(level, spawn);
        return JS_UNDEFINED;
    }

    const char *const DIFFICULTIES[] = {"Peaceful", "Easy", "Normal", "Hard"};

    JSValue worldGetDifficulty(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        const int index = (int) ScriptApi::fromContext(ctx)->host().getProperties().getDifficulty();
        return JS_NewString(ctx, DIFFICULTIES[index >= 0 && index < 4 ? index : 1]);
    }

    JSValue worldSetDifficulty(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        const std::string name = argc >= 1 && JS_IsString(argv[0]) ? toString(ctx, argv[0]) : std::string();
        for (int i = 0; i < 4; ++i) {
            if (name == DIFFICULTIES[i]) {
                ScriptApi::fromContext(ctx)->host().changeDifficulty((Difficulty) i);
                return JS_UNDEFINED;
            }
        }
        return JS_ThrowTypeError(ctx, "Native type conversion failed.");
    }

    JSValue worldGetPlayers(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        return ScriptApi::fromContext(ctx)->queryEntities(argc >= 1 ? argv[0] : JS_UNDEFINED, nullptr, true);
    }

    JSValue worldSetDynamicProperties(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        if (argc < 1 || !JS_IsObject(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (!dynamicPropertySetAll(ctx, ScriptApi::fromContext(ctx)->host().getWorldDynamicProperties(), argv[0]))
            return JS_EXCEPTION;
        return JS_UNDEFINED;
    }

    JSValue worldGetDynamicPropertyTotalByteCount(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_NewInt64(ctx,
                           dynamicPropertyByteCount(ScriptApi::fromContext(ctx)->host().getWorldDynamicProperties()));
    }

    JSValue worldGetPackSettings(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        JSValue settings = JS_NewObject(ctx);
        for (const PackSetting &setting: ScriptApi::fromContext(ctx)->packSettings()) {
            JSValue value;
            if (setting.mType == PackSetting::Type::Boolean)
                value = JS_NewBool(ctx, setting.mBoolean);
            else if (setting.mType == PackSetting::Type::Number)
                value = JS_NewFloat64(ctx, setting.mNumber);
            else
                value = string(ctx, setting.mString);
            JS_SetPropertyStr(ctx, settings, setting.mName.c_str(), value);
        }
        return settings;
    }

    // the 1.x World.playSound (sub_14C2FCE20, 26.60): every player hears it, whatever their dimension, and no
    // SoundInstance tracks it
    JSValue worldPlaySound(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 2 || !JS_IsString(argv[0]) || !readVector3(ctx, argv[1], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        ScriptSoundOptions options;
        JSValue refused = api->readSoundOptions(argc >= 3 ? argv[2] : JS_UNDEFINED, options, false);
        if (JS_IsException(refused))
            return refused;

        PlaySoundPacket packet;
        packet.mSound = toString(ctx, argv[0]);
        packet.mPosition = Vector3f((float) x, (float) y, (float) z);
        packet.mVolume = (float) options.mVolume;
        packet.mPitch = (float) options.mPitch;
        packet.mLoopCount = options.mLoopCount;
        for (auto &entry: api->host().getPlayers()) {
            if (entry.second.isSpawned())
                api->host().sendPacketTo(entry.first, packet);
        }
        return JS_UNDEFINED;
    }

    JSValue worldSendMessage(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1)
            return JS_UNDEFINED;

        bool isJson = false;
        const std::string message = messageToRawText(ctx, argv[0], isJson);
        for (auto &entry: api->host().getPlayers()) {
            if (!entry.second.isSpawned())
                continue;
            if (isJson)
                api->host().sendJsonMessage(entry.second, message);
            else
                entry.second.sendMessage(message);
        }
        return JS_UNDEFINED;
    }

    JSValue worldGetAllPlayers(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (auto &entry: api->host().getPlayers()) {
            if (entry.second.isSpawned())
                JS_SetPropertyUint32(ctx, array, index++, api->makePlayer(entry.second));
        }
        return array;
    }

    JSValue worldGetDimension(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        std::string identifier = toString(ctx, argv[0]);
        if (identifier.find(':') == std::string::npos)
            identifier = "minecraft:" + identifier;

        if (identifier == "minecraft:nether")
            return api->makeDimension(api->host().getDimension(DimensionType::Nether));
        if (identifier == "minecraft:the_end")
            return api->makeDimension(api->host().getDimension(DimensionType::TheEnd));
        if (identifier == "minecraft:overworld")
            return api->makeDimension(api->host().getDimension(DimensionType::Overworld));
        bool custom = false;
        const DimensionType dimension = Dimension::findCustom(identifier, custom);
        if (custom && api->host().getWorlds().getDefault().hasLevel(dimension))
            return api->makeDimension(api->host().getDimension(dimension));
        return JS_ThrowTypeError(ctx, "Unknown dimension %s", identifier.c_str());
    }

    JSValue worldGetDynamicProperty(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1)
            return JS_UNDEFINED;
        return dynamicPropertyGet(ctx, api->host().getWorldDynamicProperties(), toString(ctx, argv[0]));
    }

    JSValue worldSetDynamicProperty(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc >= 1)
            dynamicPropertySet(ctx, api->host().getWorldDynamicProperties(), toString(ctx, argv[0]),
                               argc >= 2 ? argv[1] : JS_UNDEFINED);
        return JS_UNDEFINED;
    }

    JSValue worldGetDynamicPropertyIds(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return dynamicPropertyIds(ctx, ScriptApi::fromContext(ctx)->host().getWorldDynamicProperties());
    }

    JSValue worldClearDynamicProperties(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ScriptApi::fromContext(ctx)->host().getWorldDynamicProperties().clear();
        return JS_UNDEFINED;
    }

    JSValue worldGetEntity(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1)
            return JS_UNDEFINED;

        const std::string idText = toString(ctx, argv[0]);
        char *end = nullptr;
        const int64_t uniqueId = (int64_t) strtoll(idText.c_str(), &end, 10);
        if (end == idText.c_str())
            return JS_UNDEFINED;

        for (auto &entry: api->host().getPlayers()) {
            if (entry.second.isSpawned() && entry.second.getUniqueId() == uniqueId)
                return api->makePlayer(entry.second);
        }
        ServerActor *actor = api->host().getActor(uniqueId);
        if (actor == nullptr || !actor->isAlive())
            return JS_UNDEFINED;
        return api->makeActor(*actor);
    }

    ScriptBlockRef *blockOf(JSContext *ctx, JSValueConst thisVal) {
        return ScriptApi::fromContext(ctx)->native<ScriptBlockRef>(thisVal);
    }

    Level &blockLevel(JSContext *ctx, const ScriptBlockRef &block) {
        return ScriptApi::fromContext(ctx)->resolveLevel(block.mWorldId, block.mDimension);
    }

    std::string blockIdentifier(JSContext *ctx, const ScriptBlockRef &block) {
        return blockLevel(ctx, block).getBlockState(block.mX, block.mY, block.mZ).mName;
    }

    JSValue blockGetCoord(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        const ScriptBlockRef *block = blockOf(ctx, thisVal);
        return JS_NewInt32(ctx, magic == 0 ? block->mX : (magic == 1 ? block->mY : block->mZ));
    }

    JSValue blockGetTypeId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return string(ctx, blockIdentifier(ctx, *blockOf(ctx, thisVal)));
    }

    // BlockType lives in the data bootstrap: the object BlockTypes.get hands out
    JSValue blockGetType(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        for (const auto &entry: api->moduleExports("@minecraft/server")) {
            if (entry.first != "BlockTypes")
                continue;
            JSValue get = JS_GetPropertyStr(ctx, entry.second, "get");
            JSValue id = string(ctx, blockIdentifier(ctx, *blockOf(ctx, thisVal)));
            JSValue type = JS_Call(ctx, get, entry.second, 1, &id);
            JS_FreeValue(ctx, id);
            JS_FreeValue(ctx, get);
            return type;
        }
        return JS_UNDEFINED;
    }

    JSValue blockGetIsAir(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewBool(ctx, blockIdentifier(ctx, *blockOf(ctx, thisVal)) == BlockNames::AIR);
    }

    JSValue blockGetIsValid(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptBlockRef *block = blockOf(ctx, thisVal);
        return JS_NewBool(ctx, blockLevel(ctx, *block).peekBlockPtr(block->mX, block->mY, block->mZ) != nullptr);
    }

    JSValue blockGetLocation(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptBlockRef *block = blockOf(ctx, thisVal);
        return vector3(ctx, (float) block->mX, (float) block->mY, (float) block->mZ);
    }

    JSValue blockGetDimension(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return ScriptApi::fromContext(ctx)->makeDimension(blockLevel(ctx, *blockOf(ctx, thisVal)));
    }

    JSValue blockCenter(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptBlockRef *block = blockOf(ctx, thisVal);
        return vector3(ctx, (float) block->mX + 0.5f, (float) block->mY + 0.5f, (float) block->mZ + 0.5f);
    }

    JSValue blockOffset(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        const ScriptBlockRef *block = blockOf(ctx, thisVal);
        int32_t steps = 1;
        if (argc >= 1 && !JS_IsUndefined(argv[0]))
            JS_ToInt32(ctx, &steps, argv[0]);

        int32_t x = block->mX, y = block->mY, z = block->mZ;
        switch (magic) {
            case 0:
                y += steps;
                break;
            case 1:
                y -= steps;
                break;
            case 2:
                z -= steps;
                break;
            case 3:
                z += steps;
                break;
            case 4:
                x += steps;
                break;
            case 5:
                x -= steps;
                break;
            default:
                break;
        }
        return ScriptApi::fromContext(ctx)->makeBlock(blockLevel(ctx, *block), x, y, z);
    }

    std::string typeIdentifier(JSContext *ctx, JSValueConst value) {
        if (!JS_IsObject(value))
            return toString(ctx, value);

        JSValue typeValue = JS_GetPropertyStr(ctx, value, "type");
        JSValue idValue = JS_GetPropertyStr(ctx, JS_IsObject(typeValue) ? typeValue : value, "id");
        const std::string identifier = toString(ctx, idValue);
        JS_FreeValue(ctx, idValue);
        JS_FreeValue(ctx, typeValue);
        return identifier;
    }

    JSValue blockSetType(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const ScriptBlockRef *block = blockOf(ctx, thisVal);
        if (argc < 1)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        ScriptApi::fromContext(ctx)->setBlockType(blockLevel(ctx, *block), block->mX, block->mY, block->mZ,
                                                  typeIdentifier(ctx, argv[0]));
        return JS_UNDEFINED;
    }

    JSValue blockSetPermutation(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        const ScriptBlockRef *block = blockOf(ctx, thisVal);
        if (argc < 1 || !JS_IsObject(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        ScriptApi *api = ScriptApi::fromContext(ctx);
        BlockState state;
        if (api->readPermutation(argv[0], state))
            api->setBlock(blockLevel(ctx, *block), block->mX, block->mY, block->mZ, state);
        return JS_UNDEFINED;
    }

    JSValue dimensionGetId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptDimensionRef *ref = ScriptApi::fromContext(ctx)->native<ScriptDimensionRef>(thisVal);
        const std::string *custom = Dimension::customName(ref->mDimension);
        if (custom != nullptr)
            return string(ctx, *custom);
        if (ref->mDimension == DimensionType::Nether)
            return JS_NewString(ctx, "minecraft:nether");
        if (ref->mDimension == DimensionType::TheEnd)
            return JS_NewString(ctx, "minecraft:the_end");
        return JS_NewString(ctx, "minecraft:overworld");
    }

    JSValue dimensionGetBlock(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 1 || !readVector3(ctx, argv[0], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        Level &level = api->levelOf(thisVal);
        const Vector3i position((int32_t) std::floor(x), (int32_t) std::floor(y), (int32_t) std::floor(z));
        if (position.y < level.getMinY() || position.y > level.getMaxY())
            return api->throwError("LocationOutOfWorldBoundariesError", "");
        // an unloaded chunk answers undefined, the way the game does
        if (level.peekBlockPtr(position.x, position.y, position.z) == nullptr)
            return JS_UNDEFINED;
        return api->makeBlock(level, position.x, position.y, position.z);
    }

    JSValue dimensionSetBlockType(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 2 || !readVector3(ctx, argv[0], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        Level &level = api->levelOf(thisVal);
        const int32_t blockY = (int32_t) std::floor(y);
        if (blockY < level.getMinY() || blockY > level.getMaxY())
            return api->throwError("LocationOutOfWorldBoundariesError", "");
        api->setBlockType(level, (int32_t) std::floor(x), blockY, (int32_t) std::floor(z),
                          typeIdentifier(ctx, argv[1]));
        return JS_UNDEFINED;
    }

    JSValue dimensionRunCommand(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        return api->runCommand(api->levelOf(thisVal), nullptr, argc >= 1 ? toString(ctx, argv[0]) : std::string());
    }

    JSValue dimensionGetPlayers(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        return api->queryEntities(argc >= 1 ? argv[0] : JS_UNDEFINED, &api->levelOf(thisVal), true);
    }

    JSValue dimensionSpawnEntity(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 2 || !readVector3(ctx, argv[1], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        ServerActor *actor = api->host().spawnActor(api->levelOf(thisVal), typeIdentifier(ctx, argv[0]),
                                                    Vector3f((float) x, (float) y, (float) z));
        // the game's messages by failure (sub_14C377CE0): only the peaceful refusal and the fallback reach here
        if (actor == nullptr)
            return api->throwError("EntitySpawnError",
                                   api->host().getProperties().getDifficulty() == Difficulty::Peaceful
                                           ? "Attempting to spawn a hostile mob in a peaceful world."
                                           : "Could not spawn Entity");
        return api->makeActor(*actor);
    }

    JSValue dimensionSpawnItem(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        const ScriptItemRef *item = argc >= 1 ? api->native<ScriptItemRef>(argv[0]) : nullptr;
        double x = 0.0, y = 0.0, z = 0.0;
        if (item == nullptr || argc < 2 || !readVector3(ctx, argv[1], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        ItemStack stack;
        if (!api->toItemStack(*item->mItem, stack))
            return JS_UNDEFINED;
        ItemActor *spawned = ItemActorHandler::dropItem(
                api->host(), api->levelOf(thisVal), Vector3f((float) x, (float) y, (float) z), stack,
                ItemActorHandler::randomDropMotion(), ItemActorHandler::DROP_PICKUP_DELAY);
        return spawned == nullptr ? JS_UNDEFINED : api->makeEntity(*spawned);
    }

    JSValue dimensionSpawnParticle(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 2 || !readVector3(ctx, argv[1], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        std::string molang;
        bool hasMolang = false;
        if (!api->readMolang(argc >= 3 ? argv[2] : JS_UNDEFINED, molang, hasMolang))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        api->host().spawnParticleEffect(api->levelOf(thisVal), toString(ctx, argv[0]),
                                        Vector3f((float) x, (float) y, (float) z), hasMolang ? &molang : nullptr,
                                        nullptr);
        return JS_UNDEFINED;
    }

    JSValue dimensionPlaySound(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        double x = 0.0, y = 0.0, z = 0.0;
        if (argc < 2 || !readVector3(ctx, argv[1], x, y, z))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        ScriptSoundOptions options;
        JSValue refused = api->readSoundOptions(argc >= 3 ? argv[2] : JS_UNDEFINED, options, true);
        if (JS_IsException(refused))
            return refused;
        return api->playSound(api->levelOf(thisVal), nullptr, toString(ctx, argv[0]),
                              Vector3f((float) x, (float) y, (float) z), options);
    }

    JSValue dimensionGetEntities(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        return api->queryEntities(argc >= 1 ? argv[0] : JS_UNDEFINED, &api->levelOf(thisVal), false);
    }

    double readNumber(JSContext *ctx, JSValueConst object, const char *key, double fallback) {
        JSValue value = JS_GetPropertyStr(ctx, object, key);
        double result = fallback;
        if (!JS_IsUndefined(value))
            JS_ToFloat64(ctx, &result, value);
        JS_FreeValue(ctx, value);
        return result;
    }

}

namespace {
    World &worldOfPack(ScriptApi &api) {
        return api.host().getWorlds().getDefault();
    }

    const TickingArea *findArea(ScriptApi &api, const std::string &name) {
        const std::vector<std::string> &names = api.tickingAreaNames();
        if (std::find(names.begin(), names.end(), name) == names.end())
            return nullptr;
        for (const TickingArea &area: worldOfPack(api).getTickingAreas().getAreas()) {
            if (area.mName == name)
                return &area;
        }
        return nullptr;
    }

    std::string areaIdentifier(JSContext *ctx, JSValueConst value) {
        if (!JS_IsObject(value))
            return toString(ctx, value);
        JSValue identifier = JS_GetPropertyStr(ctx, value, "identifier");
        const std::string name = toString(ctx, identifier);
        JS_FreeValue(ctx, identifier);
        return name;
    }

    bool areaLoaded(ScriptApi &api, const TickingArea &area) {
        Level &level = worldOfPack(api).getLevel(area.mDimension);
        for (const int64_t column: area.getColumns()) {
            const int32_t chunkX = (int32_t) (column >> 32);
            const int32_t chunkZ = (int32_t) (uint32_t) column;
            if (level.peekBlockPtr(chunkX * 16, level.getMinY(), chunkZ * 16) == nullptr)
                return false;
        }
        return true;
    }

    JSValue makeTickingArea(ScriptApi &api, const TickingArea &area) {
        JSContext *ctx = api.context();
        Level &level = worldOfPack(api).getLevel(area.mDimension);
        const std::vector<int64_t> columns = area.getColumns();
        const bool loaded = areaLoaded(api, area);

        JSValue object = JS_NewObject(ctx);
        JSValue box = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, box, "min",
                          vector3(ctx, (float) area.mMinX, (float) level.getMinY(), (float) area.mMinZ));
        JS_SetPropertyStr(ctx, box, "max",
                          vector3(ctx, (float) area.mMaxX, (float) level.getMaxY(), (float) area.mMaxZ));
        JS_SetPropertyStr(ctx, object, "boundingBox", box);
        JS_SetPropertyStr(ctx, object, "chunkCount", JS_NewInt32(ctx, (int32_t) columns.size()));
        JS_SetPropertyStr(ctx, object, "dimension", api.makeDimension(level));
        JS_SetPropertyStr(ctx, object, "identifier", string(ctx, area.mName));
        JS_SetPropertyStr(ctx, object, "isFullyLoaded", JS_NewBool(ctx, loaded));
        return object;
    }

    JSValue tickingAreaError(ScriptApi &api, const char *reason) {
        JSValue fields = JS_NewObject(api.context());
        JS_SetPropertyStr(api.context(), fields, "reason", JS_NewString(api.context(), reason));
        return api.throwError("TickingAreaError", "", fields);
    }

    JSValue tickingCreate(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        if (argc < 2 || !JS_IsObject(argv[1]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");

        const std::string name = toString(ctx, argv[0]);
        JSValue dimension = JS_GetPropertyStr(ctx, argv[1], "dimension");
        JSValue from = JS_GetPropertyStr(ctx, argv[1], "from");
        JSValue to = JS_GetPropertyStr(ctx, argv[1], "to");
        double fx = 0, fy = 0, fz = 0, tx = 0, ty = 0, tz = 0;
        const bool valid = api.native<ScriptDimensionRef>(dimension) != nullptr && readVector3(ctx, from, fx, fy, fz)
                           && readVector3(ctx, to, tx, ty, tz);
        const DimensionType type = valid ? api.levelOf(dimension).getDimensionType() : DimensionType::Overworld;
        JS_FreeValue(ctx, dimension);
        JS_FreeValue(ctx, from);
        JS_FreeValue(ctx, to);
        if (!valid)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        if (findArea(api, name) != nullptr)
            return tickingAreaError(api, "IdentifierAlreadyExists");

        TickingArea area = TickingArea::box(
                Vector3i((int32_t) std::floor(fx), (int32_t) std::floor(fy), (int32_t) std::floor(fz)),
                Vector3i((int32_t) std::floor(tx), (int32_t) std::floor(ty), (int32_t) std::floor(tz)));
        area.mName = name;
        area.mDimension = type;
        worldOfPack(api).getTickingAreas().add(worldOfPack(api), area);
        api.tickingAreaNames().push_back(name);

        // settles once every chunk of the area is loaded, checked each tick
        JSValue functions[2];
        JSValue promise = JS_NewPromiseCapability(ctx, functions);
        api.pendingTickingAreas().push_back({name, functions[0], functions[1]});
        api.resolveTickingAreas();
        return promise;
    }

    void removeAreas(ScriptApi &api, const std::function<bool(const std::string &)> &match) {
        std::vector<std::string> &names = api.tickingAreaNames();
        worldOfPack(api).getTickingAreas().remove(worldOfPack(api), [&](const TickingArea &area) {
            return std::find(names.begin(), names.end(), area.mName) != names.end() && match(area.mName);
        });
        names.erase(std::remove_if(names.begin(), names.end(), match), names.end());
    }

    JSValue tickingRemove(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        const std::string name = argc >= 1 ? areaIdentifier(ctx, argv[0]) : std::string();
        if (findArea(api, name) == nullptr)
            return tickingAreaError(api, "UnknownIdentifier");
        removeAreas(api, [&name](const std::string &candidate) { return candidate == name; });
        return JS_UNDEFINED;
    }

    JSValue tickingRemoveAll(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        removeAreas(*ScriptApi::fromContext(ctx), [](const std::string &) { return true; });
        return JS_UNDEFINED;
    }

    JSValue tickingGet(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        const TickingArea *area = argc >= 1 ? findArea(api, areaIdentifier(ctx, argv[0])) : nullptr;
        return area == nullptr ? JS_UNDEFINED : makeTickingArea(api, *area);
    }

    JSValue tickingHas(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        return JS_NewBool(ctx, argc >= 1 && findArea(api, toString(ctx, argv[0])) != nullptr);
    }

    JSValue tickingGetAll(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::string &name: api.tickingAreaNames()) {
            const TickingArea *area = findArea(api, name);
            if (area != nullptr)
                JS_SetPropertyUint32(ctx, array, index++, makeTickingArea(api, *area));
        }
        return array;
    }

    JSValue tickingChunkCount(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        int32_t count = 0;
        for (const std::string &name: api.tickingAreaNames()) {
            const TickingArea *area = findArea(api, name);
            if (area != nullptr)
                count += (int32_t) area->getColumns().size();
        }
        return JS_NewInt32(ctx, count);
    }

    // the limit is an optional<int> the level gets at creation; empty: -1 and every area fits (sub_14C3A58D0,
    // sub_14C2BBDD0). falcon sets no limit
    JSValue tickingMaxChunkCount(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        return JS_NewInt32(ctx, -1);
    }

    JSValue tickingHasCapacity(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        if (argc < 1 || !JS_IsObject(argv[0]))
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        JSValue dimension = JS_GetPropertyStr(ctx, argv[0], "dimension");
        JSValue from = JS_GetPropertyStr(ctx, argv[0], "from");
        JSValue to = JS_GetPropertyStr(ctx, argv[0], "to");
        double fx = 0, fy = 0, fz = 0, tx = 0, ty = 0, tz = 0;
        const bool valid = api.native<ScriptDimensionRef>(dimension) != nullptr && readVector3(ctx, from, fx, fy, fz)
                           && readVector3(ctx, to, tx, ty, tz);
        JS_FreeValue(ctx, dimension);
        JS_FreeValue(ctx, from);
        JS_FreeValue(ctx, to);
        if (!valid)
            return JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return JS_NewBool(ctx, true);
    }
}

void ScriptApi::resolveTickingAreas() {
    for (size_t i = 0; i < mPendingTickingAreas.size();) {
        PendingTickingArea &pending = mPendingTickingAreas[i];
        const TickingArea *area = findArea(*this, pending.mName);
        // a removed area rejects: it will never load
        if (area != nullptr && !areaLoaded(*this, *area)) {
            ++i;
            continue;
        }
        JSValue settle = area != nullptr ? pending.mResolve : pending.mReject;
        JSValue result = JS_Call(mContext, settle, JS_UNDEFINED, 0, nullptr);
        JS_FreeValue(mContext, result);
        JS_FreeValue(mContext, pending.mResolve);
        JS_FreeValue(mContext, pending.mReject);
        mPendingTickingAreas.erase(mPendingTickingAreas.begin() + (std::ptrdiff_t) i);
    }
}

void ScriptApi::_bindTickingAreas() {
    JSValue manager = defineClass("TickingAreaManager");
    property(manager, "TickingAreaManager", "chunkCount", tickingChunkCount);
    property(manager, "TickingAreaManager", "maxChunkCount", tickingMaxChunkCount);
    method(manager, "TickingAreaManager", "createTickingArea", tickingCreate, 2);
    method(manager, "TickingAreaManager", "getAllTickingAreas", tickingGetAll, 0);
    method(manager, "TickingAreaManager", "getTickingArea", tickingGet, 1);
    method(manager, "TickingAreaManager", "hasCapacity", tickingHasCapacity, 1);
    method(manager, "TickingAreaManager", "hasTickingArea", tickingHas, 1);
    method(manager, "TickingAreaManager", "removeAllTickingAreas", tickingRemoveAll, 0);
    method(manager, "TickingAreaManager", "removeTickingArea", tickingRemove, 1);
    if (mSurface.has("TickingAreaManager"))
        mTickingAreas = makeInstance("TickingAreaManager", new ScriptNative());
}

JSValue ScriptApi::queryEntities(JSValueConst options, const Level *level, bool playersOnly) {
    JSContext *ctx = mContext;
    const bool hasOptions = JS_IsObject(options);
    double centerX = 0.0, centerY = 0.0, centerZ = 0.0;
    double minDistance = -1.0, maxDistance = -1.0;
    double volumeX = 0.0, volumeY = 0.0, volumeZ = 0.0;
    bool hasVolume = false;
    double closest = 0.0, farthest = 0.0;
    if (hasOptions) {
        JSValue location = JS_GetPropertyStr(ctx, options, "location");
        readVector3(ctx, location, centerX, centerY, centerZ);
        JS_FreeValue(ctx, location);
        JSValue volume = JS_GetPropertyStr(ctx, options, "volume");
        hasVolume = JS_IsObject(volume) && readVector3(ctx, volume, volumeX, volumeY, volumeZ);
        JS_FreeValue(ctx, volume);
        minDistance = readNumber(ctx, options, "minDistance", -1.0);
        maxDistance = readNumber(ctx, options, "maxDistance", -1.0);
        closest = readNumber(ctx, options, "closest", 0.0);
        farthest = readNumber(ctx, options, "farthest", 0.0);
    }

    struct Candidate {
        JSValue mValue;
        double mDistance;
    };
    std::vector<Candidate> kept;
    JSAtom matchesAtom = JS_NewAtom(ctx, "matches");
    const auto consider = [&](JSValue candidate, const Vector3f &position) {
        const double dx = position.x - centerX, dy = position.y - centerY, dz = position.z - centerZ;
        const double distance = dx * dx + dy * dy + dz * dz;
        bool keep = (minDistance < 0.0 || distance >= minDistance * minDistance)
                    && (maxDistance < 0.0 || distance <= maxDistance * maxDistance);
        // the volume spans from the location, both ends included
        if (keep && hasVolume) {
            const double low[3] = {std::min(centerX, centerX + volumeX), std::min(centerY, centerY + volumeY),
                                   std::min(centerZ, centerZ + volumeZ)};
            const double high[3] = {std::max(centerX, centerX + volumeX), std::max(centerY, centerY + volumeY),
                                    std::max(centerZ, centerZ + volumeZ)};
            keep = position.x >= low[0] && position.x <= high[0] + 1.0 && position.y >= low[1]
                   && position.y <= high[1] + 1.0 && position.z >= low[2] && position.z <= high[2] + 1.0;
        }
        if (keep && hasOptions) {
            JSValue matches = JS_Invoke(ctx, candidate, matchesAtom, 1, &options);
            keep = JS_ToBool(ctx, matches) == 1;
            JS_FreeValue(ctx, matches);
        }
        if (keep)
            kept.push_back(Candidate{candidate, distance});
        else
            JS_FreeValue(ctx, candidate);
    };

    if (!playersOnly) {
        for (auto &entry: mHost.getActors()) {
            ServerActor &actor = *entry.second;
            if (actor.isAlive() && (level == nullptr || actor.isIn(*level)))
                consider(makeActor(actor), actor.getPosition());
        }
    }
    for (auto &entry: mHost.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (player.isSpawned() && (level == nullptr || player.isIn(*level)))
            consider(makePlayer(player), player.getPosition());
    }
    JS_FreeAtom(ctx, matchesAtom);

    size_t limit = kept.size();
    if (closest >= 1.0 || farthest >= 1.0) {
        const bool nearFirst = closest >= 1.0;
        std::stable_sort(kept.begin(), kept.end(), [nearFirst](const Candidate &left, const Candidate &right) {
            return nearFirst ? left.mDistance < right.mDistance : left.mDistance > right.mDistance;
        });
        limit = std::min(limit, (size_t) (nearFirst ? closest : farthest));
    }

    JSValue array = JS_NewArray(ctx);
    for (size_t i = 0; i < kept.size(); ++i) {
        if (i < limit)
            JS_SetPropertyUint32(ctx, array, (uint32_t) i, kept[i].mValue);
        else
            JS_FreeValue(ctx, kept[i].mValue);
    }
    return array;
}


void ScriptApi::_bindSystem() {
    JSValue system = defineClass("System");
    methodMagic(system, "System", "run", systemRun, 1, 0);
    methodMagic(system, "System", "runTimeout", systemRun, 2, 1);
    methodMagic(system, "System", "runInterval", systemRun, 2, 2);
    method(system, "System", "clearRun", systemClearRun, 1);
    property(system, "System", "currentTick", systemGetCurrentTick);
    propertyMagic(system, "System", "afterEvents", holderGetter, 2);
    propertyMagic(system, "System", "beforeEvents", holderGetter, 3);
    property(system, "System", "isEditorWorld", systemGetIsEditorWorld);
    propertyMagic(system, "System", "serverSystemInfo", holderGetter, 7);
    method(system, "System", "waitTicks", systemWaitTicks, 1);
    method(system, "System", "sendScriptEvent", systemSendScriptEvent, 2);

    if (mSurface.has("SystemInfo")) {
        JSValue info = defineClass("SystemInfo");
        property(info, "SystemInfo", "memoryTier", systemInfoGetMemoryTier);
        mSystemInfo = makeInstance("SystemInfo", new ScriptNative());
    }

    mSystem = makeInstance("System", new ScriptNative());
    if (mSurface.has("system"))
        mServerExports.emplace_back("system", JS_DupValue(mContext, mSystem));
}

void ScriptApi::_bindWorld() {
    JSValue world = defineClass("World");
    method(world, "World", "sendMessage", worldSendMessage, 1);
    method(world, "World", "playSound", worldPlaySound, 3);
    method(world, "World", "getAllPlayers", worldGetAllPlayers, 0);
    method(world, "World", "getDimension", worldGetDimension, 1);
    method(world, "World", "getEntity", worldGetEntity, 1);
    method(world, "World", "getDynamicProperty", worldGetDynamicProperty, 1);
    method(world, "World", "setDynamicProperty", worldSetDynamicProperty, 2);
    method(world, "World", "getDynamicPropertyIds", worldGetDynamicPropertyIds, 0);
    method(world, "World", "clearDynamicProperties", worldClearDynamicProperties, 0);
    propertyMagic(world, "World", "afterEvents", holderGetter, 0);
    propertyMagic(world, "World", "beforeEvents", holderGetter, 1);
    propertyMagic(world, "World", "scoreboard", holderGetter, 4);
    _bindTickingAreas();
    propertyMagic(world, "World", "tickingAreaManager", holderGetter, 5);
    propertyMagic(world, "World", "gameRules", holderGetter, 6);
    property(world, "World", "isHardcore", worldGetIsHardcore);
    property(world, "World", "seed", worldGetSeed);
    methodMagic(world, "World", "getAbsoluteTime", worldGetTime, 0, 0);
    methodMagic(world, "World", "getTimeOfDay", worldGetTime, 0, 1);
    methodMagic(world, "World", "getDay", worldGetTime, 0, 2);
    methodMagic(world, "World", "getMoonPhase", worldGetTime, 0, 3);
    methodMagic(world, "World", "setAbsoluteTime", worldSetTime, 1, 0);
    methodMagic(world, "World", "setTimeOfDay", worldSetTime, 1, 1);
    method(world, "World", "getDefaultSpawnLocation", worldGetDefaultSpawnLocation, 0);
    method(world, "World", "setDefaultSpawnLocation", worldSetDefaultSpawnLocation, 1);
    method(world, "World", "getDifficulty", worldGetDifficulty, 0);
    method(world, "World", "setDifficulty", worldSetDifficulty, 1);
    method(world, "World", "getPlayers", worldGetPlayers, 1);
    method(world, "World", "setDynamicProperties", worldSetDynamicProperties, 1);
    method(world, "World", "getDynamicPropertyTotalByteCount", worldGetDynamicPropertyTotalByteCount, 0);
    method(world, "World", "getPackSettings", worldGetPackSettings, 0);

    if (mSurface.has("GameRules")) {
        JSValue rules = defineClass("GameRules");
        for (int i = 0; i < (int) (sizeof(GAME_RULES) / sizeof(GAME_RULES[0])); ++i)
            propertyMagic(rules, "GameRules", GAME_RULES[i], gameRulesGet, i, gameRulesSet);
        mGameRules = makeInstance("GameRules", new ScriptNative());
    }

    mWorld = makeInstance("World", new ScriptNative());
    if (mSurface.has("world"))
        mServerExports.emplace_back("world", JS_DupValue(mContext, mWorld));
}

void ScriptApi::_bindBlock() {
    JSValue block = defineClass("Block");
    method(block, "Block", "setType", blockSetType, 1);
    method(block, "Block", "setPermutation", blockSetPermutation, 1);
    method(block, "Block", "center", blockCenter, 0);
    methodMagic(block, "Block", "above", blockOffset, 1, 0);
    methodMagic(block, "Block", "below", blockOffset, 1, 1);
    methodMagic(block, "Block", "north", blockOffset, 1, 2);
    methodMagic(block, "Block", "south", blockOffset, 1, 3);
    methodMagic(block, "Block", "east", blockOffset, 1, 4);
    methodMagic(block, "Block", "west", blockOffset, 1, 5);
    property(block, "Block", "location", blockGetLocation);
    property(block, "Block", "dimension", blockGetDimension);
    property(block, "Block", "isValid", blockGetIsValid);
    propertyMagic(block, "Block", "x", blockGetCoord, 0);
    propertyMagic(block, "Block", "y", blockGetCoord, 1);
    propertyMagic(block, "Block", "z", blockGetCoord, 2);
    property(block, "Block", "typeId", blockGetTypeId);
    property(block, "Block", "type", blockGetType);
    property(block, "Block", "isAir", blockGetIsAir);

    _bindBlockSurface();
}

void ScriptApi::_bindDimension() {
    JSValue dimension = defineClass("Dimension");
    property(dimension, "Dimension", "id", dimensionGetId);
    method(dimension, "Dimension", "getBlock", dimensionGetBlock, 1);
    method(dimension, "Dimension", "setBlockType", dimensionSetBlockType, 2);
    method(dimension, "Dimension", "runCommand", dimensionRunCommand, 1);
    method(dimension, "Dimension", "getPlayers", dimensionGetPlayers, 1);
    method(dimension, "Dimension", "spawnEntity", dimensionSpawnEntity, 3);
    method(dimension, "Dimension", "spawnItem", dimensionSpawnItem, 2);
    method(dimension, "Dimension", "spawnParticle", dimensionSpawnParticle, 3);
    method(dimension, "Dimension", "playSound", dimensionPlaySound, 3);
    method(dimension, "Dimension", "getEntities", dimensionGetEntities, 1);
    _bindDimensionSurface();
}
