#include "Scripting/Binding/ScriptApi.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/Binding/ScriptRayQueries.h"
#include "Scripting/ScriptEngine.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Biome/BiomeRegistry.h"
#include "World/Level/Block/Actor/BlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Explosion.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/Biome/BiomeSearch.h"
#include "World/Level/LevelGen/Feature/ChunkGenerateContext.h"
#include "World/Level/LevelGen/Feature/GenerateFeatureRegistry.h"
#include "World/Level/LevelGen/Feature/GeneratorRoot.h"
#include "World/Level/LevelGen/Feature/Tree/LegacyTreeObject.h"
#include "World/Level/LevelGen/Random/SimpleRandom.h"
#include "World/Level/World.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_set>

using namespace ScriptValues;

namespace {
    Level &levelOf(JSContext *ctx, JSValueConst thisVal) {
        return ScriptApi::fromContext(ctx)->levelOf(thisVal);
    }

    bool readBlockLocation(JSContext *ctx, JSValueConst value, Vector3i &out) {
        double x = 0.0, y = 0.0, z = 0.0;
        if (!readVector3(ctx, value, x, y, z) || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            return false;
        out = Vector3i((int32_t) std::floor(x), (int32_t) std::floor(y), (int32_t) std::floor(z));
        return true;
    }

    bool readPoint(JSContext *ctx, JSValueConst value, Vector3f &out) {
        double x = 0.0, y = 0.0, z = 0.0;
        if (!readVector3(ctx, value, x, y, z) || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            return false;
        out = Vector3f((float) x, (float) y, (float) z);
        return true;
    }

    bool inHeight(const Level &level, int32_t y) {
        return y >= level.getMinY() && y <= level.getMaxY();
    }

    bool loaded(Level &level, const Vector3i &position) {
        return level.peekChunkPtr(position.x >> 4, position.z >> 4) != nullptr;
    }

    JSValue typeError(JSContext *ctx) {
        return JS_ThrowTypeError(ctx, "Native type conversion failed.");
    }

    // the requested block as a state: a BlockPermutation, a BlockType or a type id
    bool readBlockArgument(ScriptApi &api, JSValueConst value, BlockState &state) {
        JSContext *ctx = api.context();
        if (api.readPermutation(value, state))
            return true;
        std::string identifier;
        if (JS_IsString(value)) {
            identifier = toString(ctx, value);
        } else if (JS_IsObject(value)) {
            JSValue id = JS_GetPropertyStr(ctx, value, "id");
            identifier = JS_IsString(id) ? toString(ctx, id) : std::string();
            JS_FreeValue(ctx, id);
        }
        if (identifier.empty())
            return false;
        if (identifier.find(':') == std::string::npos)
            identifier = "minecraft:" + identifier;
        state = ScriptApi::defaultBlockState(identifier);
        return true;
    }

    JSValue dimensionGetHeightRange(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const Level &level = levelOf(ctx, thisVal);
        JSValue range = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, range, "min", JS_NewInt32(ctx, level.getMinY()));
        // sub_14B5B5520 returns the stored bound: exclusive, the top block is max - 1 (0x143848dbc)
        JS_SetPropertyStr(ctx, range, "max", JS_NewInt32(ctx, level.getMaxY() + 1));
        return range;
    }

    // dimension.dimensionName{} in the 26.60 strings: the id fills the braces
    JSValue dimensionGetLocalizationKey(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return string(ctx, "dimension.dimensionName" + std::to_string(levelOf(ctx, thisVal).getDimensionId()));
    }

    Vector3f normalizedDirection(const Vector3f &direction) {
        const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y
                                       + direction.z * direction.z);
        return length > 0.0f ? Vector3f(direction.x / length, direction.y / length, direction.z / length) : direction;
    }

    JSValue dimensionGetBlockFromRay(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        Vector3f origin, direction;
        if (argc < 2 || !readPoint(ctx, argv[0], origin) || !readPoint(ctx, argv[1], direction))
            return typeError(ctx);
        return ScriptRayQueries::blockFromRay(ctx, levelOf(ctx, thisVal), origin, normalizedDirection(direction),
                                              argc >= 3 ? argv[2] : JS_UNDEFINED);
    }

    JSValue dimensionGetEntitiesFromRay(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        Vector3f origin, direction;
        if (argc < 2 || !readPoint(ctx, argv[0], origin) || !readPoint(ctx, argv[1], direction))
            return typeError(ctx);
        return ScriptRayQueries::entitiesFromRay(ctx, levelOf(ctx, thisVal), origin, normalizedDirection(direction),
                                                 argc >= 3 ? argv[2] : JS_UNDEFINED, nullptr);
    }

    // a vertical ray from just past the block's face: the block itself is never the hit
    JSValue dimensionGetBlockVertical(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        Vector3i at;
        if (argc < 1 || !readBlockLocation(ctx, argv[0], at))
            return typeError(ctx);
        const bool up = magic == 0;
        const Vector3f origin((float) at.x + 0.5f, up ? (float) at.y + 1.001f : (float) at.y - 0.001f,
                              (float) at.z + 0.5f);
        JSValue hit = ScriptRayQueries::blockFromRay(ctx, levelOf(ctx, thisVal), origin,
                                                     Vector3f(0.0f, up ? 1.0f : -1.0f, 0.0f),
                                                     argc >= 2 ? argv[1] : JS_UNDEFINED);
        if (!JS_IsObject(hit))
            return hit;
        JSValue block = JS_GetPropertyStr(ctx, hit, "block");
        JS_FreeValue(ctx, hit);
        return block;
    }

    JSValue dimensionLight(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        Vector3i at;
        if (argc < 1 || !readBlockLocation(ctx, argv[0], at))
            return typeError(ctx);
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Level &level = levelOf(ctx, thisVal);
        if (!inHeight(level, at.y))
            return api->throwError("LocationOutOfWorldBoundariesError", "");
        if (!loaded(level, at))
            return api->throwError("LocationInUnloadedChunkError", "");
        const int sky = level.getSkyLightAt(at.x, at.y, at.z);
        if (magic == 1)
            return JS_NewInt32(ctx, sky);
        // TODO: same combination as Block.getLightLevel, unverified
        return JS_NewInt32(ctx, std::max(level.getBlockLightAt(at.x, at.y, at.z),
                                         std::max(sky - level.getSkyLightSubtracted(), 0)));
    }

    JSValue dimensionGetTopmostBlock(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Level &level = levelOf(ctx, thisVal);
        if (argc < 1 || !JS_IsObject(argv[0]))
            return typeError(ctx);
        double x = 0.0, z = 0.0;
        JSValue xValue = JS_GetPropertyStr(ctx, argv[0], "x");
        JSValue zValue = JS_GetPropertyStr(ctx, argv[0], "z");
        const bool valid = JS_ToFloat64(ctx, &x, xValue) == 0 && JS_ToFloat64(ctx, &z, zValue) == 0;
        JS_FreeValue(ctx, xValue);
        JS_FreeValue(ctx, zValue);
        if (!valid)
            return typeError(ctx);
        int32_t minHeight = level.getMinY();
        if (argc >= 2 && !JS_IsUndefined(argv[1]))
            JS_ToInt32(ctx, &minHeight, argv[1]);
        const int32_t bx = (int32_t) std::floor(x), bz = (int32_t) std::floor(z);
        if (!loaded(level, Vector3i(bx, 0, bz)))
            return JS_UNDEFINED;
        for (int32_t y = level.getMaxY(); y >= std::max(minHeight, level.getMinY()); --y) {
            const BlockState *state = level.peekBlockPtr(bx, y, bz);
            if (state != nullptr && state->mName != "minecraft:air")
                return api->makeBlock(level, bx, y, bz);
        }
        return JS_UNDEFINED;
    }

    JSValue dimensionIsChunkLoaded(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        Vector3i at;
        if (argc < 1 || !readBlockLocation(ctx, argv[0], at))
            return typeError(ctx);
        return JS_NewBool(ctx, loaded(levelOf(ctx, thisVal), at));
    }

    JSValue dimensionSetBlockPermutation(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Vector3i at;
        BlockState state;
        if (argc < 2 || !readBlockLocation(ctx, argv[0], at) || !api->readPermutation(argv[1], state))
            return typeError(ctx);
        Level &level = levelOf(ctx, thisVal);
        if (!inHeight(level, at.y))
            return api->throwError("LocationOutOfWorldBoundariesError", "");
        if (!loaded(level, at))
            return api->throwError("LocationInUnloadedChunkError", "");
        api->setBlock(level, at.x, at.y, at.z, state);
        return JS_UNDEFINED;
    }

    JSValue dimensionGetEntitiesAtBlockLocation(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Vector3i at;
        if (argc < 1 || !readBlockLocation(ctx, argv[0], at))
            return typeError(ctx);
        Level &level = levelOf(ctx, thisVal);
        const auto inside = [&](const Vector3f &position) {
            return (int32_t) std::floor(position.x) == at.x && (int32_t) std::floor(position.y) == at.y
                   && (int32_t) std::floor(position.z) == at.z;
        };
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (auto &entry: api->host().getActors()) {
            ServerActor &actor = *entry.second;
            if (actor.isAlive() && actor.isIn(level) && inside(actor.getPosition()))
                JS_SetPropertyUint32(ctx, array, index++, api->makeActor(actor));
        }
        for (auto &entry: api->host().getPlayers()) {
            ServerPlayer &player = entry.second;
            if (player.isSpawned() && player.isIn(level) && inside(player.getPosition()))
                JS_SetPropertyUint32(ctx, array, index++, api->makePlayer(player));
        }
        return array;
    }

    // the volume's locations; unloaded ones throw UnloadedChunksError unless allowed
    bool volumeLocations(ScriptApi &api, Level &level, JSValueConst volume, bool allowUnloaded,
                         std::vector<Vector3i> &locations, JSValue &error) {
        error = JS_UNDEFINED;
        if (!api.readVolume(volume, locations)) {
            error = JS_EXCEPTION;
            return false;
        }
        for (const Vector3i &location: locations) {
            if (!inHeight(level, location.y)) {
                error = api.throwError("LocationOutOfWorldBoundariesError", "");
                return false;
            }
            if (!allowUnloaded && !loaded(level, location)) {
                error = api.throwError("UnloadedChunksError", "");
                return false;
            }
        }
        return true;
    }

    JSValue dimensionGetBlocks(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 2 || !JS_IsObject(argv[0]))
            return typeError(ctx);
        Level &level = levelOf(ctx, thisVal);
        const bool allowUnloaded = argc >= 3 && JS_ToBool(ctx, argv[2]) == 1;
        std::vector<Vector3i> locations;
        JSValue error;
        if (!volumeLocations(*api, level, argv[0], allowUnloaded, locations, error))
            return error;
        std::vector<Vector3i> kept;
        for (const Vector3i &location: locations) {
            const BlockState *state = level.peekBlockPtr(location.x, location.y, location.z);
            if (state != nullptr && ScriptRayQueries::blockPassesFilter(ctx, *state, argv[1]))
                kept.push_back(location);
        }
        return api->makeListVolume(kept);
    }

    JSValue dimensionContainsBlock(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 2 || !JS_IsObject(argv[0]))
            return typeError(ctx);
        Level &level = levelOf(ctx, thisVal);
        const bool allowUnloaded = argc >= 3 && JS_ToBool(ctx, argv[2]) == 1;
        std::vector<Vector3i> locations;
        JSValue error;
        if (!volumeLocations(*api, level, argv[0], allowUnloaded, locations, error))
            return error;
        for (const Vector3i &location: locations) {
            const BlockState *state = level.peekBlockPtr(location.x, location.y, location.z);
            if (state != nullptr && ScriptRayQueries::blockPassesFilter(ctx, *state, argv[1]))
                return JS_TRUE;
        }
        return JS_FALSE;
    }

    JSValue dimensionFillBlocks(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        BlockState state;
        if (argc < 2 || !JS_IsObject(argv[0]) || !readBlockArgument(*api, argv[1], state))
            return typeError(ctx);
        Level &level = levelOf(ctx, thisVal);
        JSValueConst options = argc >= 3 ? argv[2] : JS_UNDEFINED;
        JSValue filter = JS_IsObject(options) ? JS_GetPropertyStr(ctx, options, "blockFilter") : JS_UNDEFINED;
        bool ignoreUnloaded = false;
        if (JS_IsObject(options)) {
            JSValue ignore = JS_GetPropertyStr(ctx, options, "ignoreChunkBoundErrors");
            ignoreUnloaded = JS_ToBool(ctx, ignore) == 1;
            JS_FreeValue(ctx, ignore);
        }
        std::vector<Vector3i> locations;
        JSValue error;
        if (!volumeLocations(*api, level, argv[0], ignoreUnloaded, locations, error)) {
            JS_FreeValue(ctx, filter);
            return error;
        }
        std::vector<Vector3i> placed;
        for (const Vector3i &location: locations) {
            const BlockState *current = level.peekBlockPtr(location.x, location.y, location.z);
            if (current == nullptr
                || (JS_IsObject(filter) && !ScriptRayQueries::blockPassesFilter(ctx, *current, filter)))
                continue;
            api->setBlock(level, location.x, location.y, location.z, state);
            placed.push_back(location);
        }
        JS_FreeValue(ctx, filter);
        return api->makeListVolume(placed);
    }

    JSValue dimensionCloneBlocks(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Vector3i begin, end, destination;
        int32_t mode = 0;
        if (argc < 4 || !readBlockLocation(ctx, argv[0], begin) || !readBlockLocation(ctx, argv[1], end)
            || !readBlockLocation(ctx, argv[2], destination) || JS_ToInt32(ctx, &mode, argv[3]) != 0)
            return typeError(ctx);
        Level &level = levelOf(ctx, thisVal);
        JSValueConst filter = argc >= 5 ? argv[4] : JS_UNDEFINED;
        const Vector3i low(std::min(begin.x, end.x), std::min(begin.y, end.y), std::min(begin.z, end.z));
        const Vector3i high(std::max(begin.x, end.x), std::max(begin.y, end.y), std::max(begin.z, end.z));
        const Vector3i span(high.x - low.x + 1, high.y - low.y + 1, high.z - low.z + 1);
        const bool overlaps = destination.x <= high.x && destination.x + span.x - 1 >= low.x && destination.y <= high.y
                              && destination.y + span.y - 1 >= low.y && destination.z <= high.z
                              && destination.z + span.z - 1 >= low.z;
        // Copy refuses an overlap, ForceCopy and Move take it
        // TODO: the game's error for an overlapping Copy is unverified
        if (mode == 0 && overlaps)
            return api->throwError("Error", "The source and destination regions overlap.");

        struct Copied {
            Vector3i mOffset;
            BlockState mState;
            bool mHasActor = false;
            Tag mActor;
        };
        std::vector<Copied> copied;
        for (int32_t x = low.x; x <= high.x; ++x) {
            for (int32_t y = low.y; y <= high.y; ++y) {
                for (int32_t z = low.z; z <= high.z; ++z) {
                    if (!inHeight(level, y) || !loaded(level, Vector3i(x, y, z)))
                        return api->throwError("UnloadedChunksError", "");
                    const BlockState state = level.getBlockState(x, y, z);
                    if (JS_IsObject(filter) && !ScriptRayQueries::blockPassesFilter(ctx, state, filter))
                        continue;
                    Copied entry;
                    entry.mOffset = Vector3i(x - low.x, y - low.y, z - low.z);
                    entry.mState = state;
                    if (BlockActor *actor = level.getBlockActors().find(Vector3i(x, y, z))) {
                        entry.mHasActor = true;
                        entry.mActor = actor->saveNbt();
                        entry.mActor.putString("id", actor->getBlockActorId());
                    }
                    copied.push_back(entry);
                }
            }
        }
        if (mode == 2) {
            for (const Copied &entry: copied) {
                const Vector3i source(low.x + entry.mOffset.x, low.y + entry.mOffset.y, low.z + entry.mOffset.z);
                level.getBlockActors().remove(source);
                api->setBlock(level, source.x, source.y, source.z, BlockState("minecraft:air"));
            }
        }
        for (const Copied &entry: copied) {
            const Vector3i target(destination.x + entry.mOffset.x, destination.y + entry.mOffset.y,
                                  destination.z + entry.mOffset.z);
            if (!inHeight(level, target.y) || !loaded(level, target))
                continue;
            level.getBlockActors().remove(target);
            api->setBlock(level, target.x, target.y, target.z, entry.mState);
            std::unique_ptr<BlockActor> actor = entry.mHasActor ? BlockActorStore::create(entry.mActor.getString("id"))
                                                                : nullptr;
            if (actor == nullptr)
                continue;
            actor->loadNbt(entry.mActor, api->host().getCodecContext());
            actor->setPosition(target);
            BlockActor &placed = *actor;
            level.getBlockActors().insert(std::move(actor));
            BlockActionHandler::broadcastBlockActorData(api->host(), level, placed);
        }
        return JS_UNDEFINED;
    }

    bool readBool(JSContext *ctx, JSValueConst options, const char *key, bool fallback) {
        if (!JS_IsObject(options))
            return fallback;
        JSValue value = JS_GetPropertyStr(ctx, options, key);
        const bool result = JS_IsUndefined(value) ? fallback : JS_ToBool(ctx, value) == 1;
        JS_FreeValue(ctx, value);
        return result;
    }

    JSValue dimensionCreateExplosion(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Vector3f center;
        double radius = 0.0;
        if (argc < 2 || !readPoint(ctx, argv[0], center) || JS_ToFloat64(ctx, &radius, argv[1]) != 0
            || !std::isfinite(radius))
            return typeError(ctx);
        // TODO: the game's radius bounds are unverified; a negative one is refused
        if (radius < 0.0)
            return api->throwError("ArgumentOutOfBoundsError", "");
        Level &level = levelOf(ctx, thisVal);
        JSValueConst options = argc >= 3 ? argv[2] : JS_UNDEFINED;
        const ServerActor *source = nullptr;
        if (JS_IsObject(options)) {
            JSValue sourceValue = JS_GetPropertyStr(ctx, options, "source");
            source = api->resolveActor(sourceValue);
            JS_FreeValue(ctx, sourceValue);
        }
        Explosion explosion(api->host(), level, center, radius, source, false);
        explosion.setIncendiary(readBool(ctx, options, "causesFire", false));
        if (readBool(ctx, options, "allowUnderwater", false))
            explosion.setInWater(false);
        if (!readBool(ctx, options, "breaksBlocks", true))
            return JS_NewBool(ctx, explosion.explodeWithoutBlocks());
        return JS_NewBool(ctx, explosion.explode());
    }

    JSValue dimensionSetWeather(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1 || !JS_IsString(argv[0]))
            return typeError(ctx);
        const std::string type = toString(ctx, argv[0]);
        if (type != "Clear" && type != "Rain" && type != "Thunder")
            return typeError(ctx);
        int32_t duration;
        if (argc >= 2 && !JS_IsUndefined(argv[1])) {
            if (JS_ToInt32(ctx, &duration, argv[1]) != 0)
                return typeError(ctx);
        } else {
            // sub_14B5B4B90: 20 * nextInt(600) + 6000 ticks
            SimpleRandom random((int64_t) std::chrono::steady_clock::now().time_since_epoch().count());
            duration = 20 * random.nextInt(600) + 6000;
        }
        if (duration < 1)
            return api->throwError("ArgumentOutOfBoundsError", "");

        ServerNetworkHandler &host = api->host();
        World &world = host.getWorldOf(levelOf(ctx, thisVal));
        Level &overworld = world.getOverworld();
        host.setThundering(world, type == "Thunder");
        host.setRaining(world, type != "Clear");
        overworld.setRainTime(duration);
        if (type != "Rain")
            overworld.setThunderTime(duration);
        host.broadcastWeather(world);
        return JS_UNDEFINED;
    }

    JSValue dimensionSpawnXp(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Vector3f at;
        int32_t amount = 0;
        if (argc < 2 || !readPoint(ctx, argv[0], at) || JS_ToInt32(ctx, &amount, argv[1]) != 0)
            return typeError(ctx);
        if (amount < 0)
            return api->throwError("ArgumentOutOfBoundsError", "");
        api->host().spawnExperienceOrbs(levelOf(ctx, thisVal), at, amount);
        return JS_UNDEFINED;
    }

    // biomes: BiomeType carries the name without namespace; the registry knows the tags
    std::string biomeTypeId(const std::string &name) {
        return name.find(':') == std::string::npos ? "minecraft:" + name : name;
    }

    std::string biomeShortName(const std::string &id) {
        return id.compare(0, 10, "minecraft:") == 0 ? id.substr(10) : id;
    }

    JSValue makeBiomeType(ScriptApi &api, const std::string &name) {
        ScriptNamedRef *ref = new ScriptNamedRef();
        ref->mName = biomeTypeId(name);
        return api.makeInstance("BiomeType", ref);
    }

    std::vector<std::string> biomeTags(ScriptApi &api, const std::string &id) {
        std::vector<std::string> tags;
        const BiomeRegistry &registry = api.host().getBiomes();
        const std::string name = biomeShortName(id);
        for (const BiomeDefinitionData &biome: registry.getBiomes()) {
            if (biome.mName != name && biome.mName != id)
                continue;
            if (biome.mTagIndexes.has_value()) {
                for (const uint16_t index: *biome.mTagIndexes) {
                    if (index < registry.getStrings().size())
                        tags.push_back(registry.getStrings()[index]);
                }
            }
            break;
        }
        return tags;
    }

    JSValue biomeGetId(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return string(ctx, ScriptApi::fromContext(ctx)->native<ScriptNamedRef>(thisVal)->mName);
    }

    JSValue biomeGetTags(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::string &tag: biomeTags(*api, api->native<ScriptNamedRef>(thisVal)->mName))
            JS_SetPropertyUint32(ctx, array, index++, string(ctx, tag));
        return array;
    }

    JSValue biomeHasTags(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1 || !JS_IsArray(argv[0]))
            return typeError(ctx);
        const std::vector<std::string> tags = biomeTags(*api, api->native<ScriptNamedRef>(thisVal)->mName);
        uint32_t length = 0;
        JSValue lengthValue = JS_GetPropertyStr(ctx, argv[0], "length");
        JS_ToUint32(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);
        for (uint32_t i = 0; i < length; ++i) {
            JSValue entry = JS_GetPropertyUint32(ctx, argv[0], i);
            const std::string tag = toString(ctx, entry);
            JS_FreeValue(ctx, entry);
            if (std::find(tags.begin(), tags.end(), tag) == tags.end())
                return JS_FALSE;
        }
        return JS_TRUE;
    }

    JSValue biomeTypesGet(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 1 || !JS_IsString(argv[0]))
            return typeError(ctx);
        BiomeChunkGenDataRegistry::initialize();
        const std::string name = toString(ctx, argv[0]);
        if (BiomeChunkGenDataRegistry::getBiomeId(name) < 0)
            return JS_UNDEFINED;
        return makeBiomeType(*api, biomeShortName(biomeTypeId(name)));
    }

    JSValue biomeTypesGetAll(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        BiomeChunkGenDataRegistry::initialize();
        JSValue array = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::string &name: BiomeChunkGenDataRegistry::getBiomeNames())
            JS_SetPropertyUint32(ctx, array, index++, makeBiomeType(*api, name));
        return array;
    }

    std::string biomeNameById(int32_t id) {
        BiomeChunkGenDataRegistry::initialize();
        for (const std::string &name: BiomeChunkGenDataRegistry::getBiomeNames()) {
            if (BiomeChunkGenDataRegistry::getBiomeId(name) == id)
                return name;
        }
        return std::string();
    }

    JSValue dimensionGetBiome(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Vector3i at;
        if (argc < 1 || !readBlockLocation(ctx, argv[0], at))
            return typeError(ctx);
        Level &level = levelOf(ctx, thisVal);
        if (!inHeight(level, at.y))
            return api->throwError("LocationOutOfWorldBoundariesError", "");
        LevelChunk *chunk = level.peekChunkPtr(at.x >> 4, at.z >> 4);
        const int32_t id = chunk != nullptr ? (int32_t) chunk->getBiomeAt(at.x & 15, at.y, at.z & 15)
                                            : level.pickBiome(at.x, at.y, at.z);
        const std::string name = biomeNameById(id);
        if (name.empty())
            return JS_UNDEFINED;
        return makeBiomeType(*api, name);
    }

    JSValue dimensionClosestBiome(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Vector3i at;
        if (argc < 2 || !readBlockLocation(ctx, argv[0], at))
            return typeError(ctx);
        std::string name;
        if (JS_IsString(argv[1])) {
            name = toString(ctx, argv[1]);
        } else if (const ScriptNamedRef *biome = api->native<ScriptNamedRef>(argv[1])) {
            name = biome->mName;
        } else {
            return typeError(ctx);
        }
        BiomeChunkGenDataRegistry::initialize();
        const int32_t id = BiomeChunkGenDataRegistry::getBiomeId(name);
        if (id < 0)
            return api->throwError("Error", "Unknown biome " + name);
        // TODO: boundingSize of BiomeSearchOptions is not applied; the /locate search radius is used
        const BiomeSearch::Result result = BiomeSearch::findNearest(levelOf(ctx, thisVal).getGenerator(), at, id);
        if (!result.mFound)
            return JS_UNDEFINED;
        return vector3(ctx, (float) result.mPosition.x, (float) result.mPosition.y, (float) result.mPosition.z);
    }

    std::vector<std::string> stringList(JSContext *ctx, JSValueConst object, const char *key) {
        std::vector<std::string> values;
        JSValue list = JS_IsObject(object) ? JS_GetPropertyStr(ctx, object, key) : JS_UNDEFINED;
        if (JS_IsArray(list)) {
            uint32_t length = 0;
            JSValue lengthValue = JS_GetPropertyStr(ctx, list, "length");
            JS_ToUint32(ctx, &length, lengthValue);
            JS_FreeValue(ctx, lengthValue);
            for (uint32_t index = 0; index < length; ++index) {
                JSValue entry = JS_GetPropertyUint32(ctx, list, index);
                values.push_back(toString(ctx, entry));
                JS_FreeValue(ctx, entry);
            }
        }
        JS_FreeValue(ctx, list);
        return values;
    }

    bool contains(const std::vector<std::string> &list, const std::string &value) {
        return std::find(list.begin(), list.end(), value) != list.end();
    }

    // the rule as the definitions word it: excluded biomes or tags fail at once; a superset needs one included
    // biome, else every biome must be included
    // TODO: the sampling (every block) and the reading of the include rule are unverified in the game
    JSValue dimensionContainsBiomes(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        if (argc < 3 || !JS_IsObject(argv[0]) || !JS_IsObject(argv[1]))
            return typeError(ctx);
        Level &level = levelOf(ctx, thisVal);
        const bool superset = JS_ToBool(ctx, argv[2]) == 1;
        std::vector<Vector3i> locations;
        if (!api->readVolume(argv[0], locations))
            return JS_EXCEPTION;
        const std::vector<std::string> includeBiomes = stringList(ctx, argv[1], "includeBiomes");
        const std::vector<std::string> excludeBiomes = stringList(ctx, argv[1], "excludeBiomes");
        const std::vector<std::string> includeTags = stringList(ctx, argv[1], "includeTags");
        const std::vector<std::string> excludeTags = stringList(ctx, argv[1], "excludeTags");

        std::vector<std::string> biomes;
        for (const Vector3i &location: locations) {
            // only the part inside the world is searched
            if (!inHeight(level, location.y))
                continue;
            LevelChunk *chunk = level.peekChunkPtr(location.x >> 4, location.z >> 4);
            const int32_t id = chunk != nullptr
                                       ? (int32_t) chunk->getBiomeAt(location.x & 15, location.y, location.z & 15)
                                       : level.pickBiome(location.x, location.y, location.z);
            const std::string name = biomeTypeId(biomeNameById(id));
            if (!contains(biomes, name))
                biomes.push_back(name);
        }

        const auto named = [](const std::vector<std::string> &list, const std::string &id) {
            return contains(list, id) || contains(list, biomeShortName(id));
        };
        bool anyIncluded = false, allIncluded = true;
        for (const std::string &biome: biomes) {
            const std::vector<std::string> tags = biomeTags(*api, biome);
            if (named(excludeBiomes, biome))
                return JS_FALSE;
            for (const std::string &tag: excludeTags) {
                if (contains(tags, tag))
                    return JS_FALSE;
            }
            bool allTags = !includeTags.empty();
            for (const std::string &tag: includeTags)
                allTags = allTags && contains(tags, tag);
            const bool listed = named(includeBiomes, biome);
            const bool included = superset ? (listed || allTags)
                                           : ((includeBiomes.empty() || listed) && (includeTags.empty() || allTags));
            anyIncluded = anyIncluded || included;
            allIncluded = allIncluded && included;
        }
        if (biomes.empty())
            return JS_FALSE;
        return JS_NewBool(ctx, superset ? anyIncluded : allIncluded);
    }

    // features: GeneratorRoot writes the level, the tracking copy tells the clients
    class TrackingRoot final : public GeneratorRoot {
    public:
        explicit TrackingRoot(Level &level) : GeneratorRoot(level) {}

        void setBlockStateAt(int32_t x, int32_t y, int32_t z, const BlockState &state) override {
            GeneratorRoot::setBlockStateAt(x, y, z, state);
            mChanged.emplace_back(x, y, z);
        }

        std::vector<Vector3i> mChanged;
    };

    void broadcastChanged(ScriptApi &api, Level &level, const std::vector<Vector3i> &changed) {
        std::unordered_set<int64_t> seen;
        for (const Vector3i &position: changed) {
            if (!seen.insert(BlockActorStore::packPosition(position)).second)
                continue;
            BlockActionHandler::broadcastBlockUpdate(api.host(), level, position,
                                                     level.getBlockState(position.x, position.y, position.z));
        }
    }

    struct TreeFeature {
        const char *mName;
        TreeWoodType mType;
    };

    // features of the 26.60 strings that the sapling trees draw; their data-driven shapes are not loaded
    // TODO: the game's own shape of each feature is unverified, the sapling tree of the same wood grows instead
    const TreeFeature TREE_FEATURES[] = {
            {"minecraft:select_oak_tree_feature", TreeWoodType::OAK},
            {"minecraft:birch_tree_feature", TreeWoodType::BIRCH},
            {"minecraft:select_spruce_tree_feature", TreeWoodType::SPRUCE},
            {"minecraft:select_jungle_tree_feature", TreeWoodType::JUNGLE},
            {"minecraft:savanna_tree_feature", TreeWoodType::ACACIA},
            {"minecraft:select_roofed_tree_feature", TreeWoodType::DARK_OAK},
            {"minecraft:cherry_tree_feature", TreeWoodType::CHERRY},
            {"minecraft:pale_oak_tree_feature", TreeWoodType::PALE_OAK},
    };

    JSValue dimensionPlaceFeature(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Vector3i at;
        if (argc < 2 || !JS_IsString(argv[0]) || !readBlockLocation(ctx, argv[1], at))
            return typeError(ctx);
        const bool shouldThrow = argc >= 3 && JS_ToBool(ctx, argv[2]) == 1;
        std::string name = toString(ctx, argv[0]);
        if (name.find(':') == std::string::npos)
            name = "minecraft:" + name;
        Level &level = levelOf(ctx, thisVal);
        if (!inHeight(level, at.y))
            return api->throwError("LocationOutOfWorldBoundariesError", "");
        if (!loaded(level, at))
            return api->throwError("LocationInUnloadedChunkError", "");

        const TreeFeature *tree = nullptr;
        for (const TreeFeature &candidate: TREE_FEATURES) {
            if (name == candidate.mName)
                tree = &candidate;
        }
        // sub_14B5C5B50
        if (tree == nullptr)
            return api->throwError("InvalidArgumentError", "Feature name " + name + " cannot be found in the registry");

        TrackingRoot root(level);
        SimpleRandom random((int64_t) std::chrono::steady_clock::now().time_since_epoch().count());
        LegacyTreeObject::growTree(root, at.x, at.y, at.z, random, tree->mType, false);
        if (root.mChanged.empty()) {
            if (shouldThrow)
                return api->throwError("Error", "Feature " + name + " could not be placed.");
            return JS_FALSE;
        }
        const std::vector<Vector3i> changed = root.mChanged;
        root.applySubChunkUpdate();
        broadcastChanged(*api, level, changed);
        return JS_TRUE;
    }

    // the rule scatters over the chunk holding the location, as world generation runs it
    JSValue dimensionPlaceFeatureRule(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv) {
        ScriptApi *api = ScriptApi::fromContext(ctx);
        Vector3i at;
        if (argc < 2 || !JS_IsString(argv[0]) || !readBlockLocation(ctx, argv[1], at))
            return typeError(ctx);
        std::string name = toString(ctx, argv[0]);
        if (name.find(':') == std::string::npos)
            name = "minecraft:" + name;
        Level &level = levelOf(ctx, thisVal);
        LevelChunk *chunk = level.peekChunkPtr(at.x >> 4, at.z >> 4);
        if (chunk == nullptr)
            return api->throwError("LocationInUnloadedChunkError", "");

        BiomeChunkGenDataRegistry::initialize();
        GenerateFeatureRegistry::initialize();
        IFeaturePtr feature;
        for (const std::string &biome: BiomeChunkGenDataRegistry::getBiomeNames()) {
            const std::vector<BiomeFeatureEntry> *entries = BiomeChunkGenDataRegistry::getConsolidatedFeatures(
                    BiomeChunkGenDataRegistry::getBiomeId(biome));
            if (entries == nullptr)
                continue;
            for (const BiomeFeatureEntry &entry: *entries) {
                if (entry.mIdentifier != name)
                    continue;
                feature = GenerateFeatureRegistry::get(entry.mIdentifier);
                if (feature == nullptr)
                    feature = GenerateFeatureRegistry::get(entry.mFeature);
                break;
            }
            if (feature != nullptr)
                break;
        }
        // sub_14B5C7EC0
        if (feature == nullptr)
            return api->throwError("InvalidArgumentError", "Feature rule " + name + " cannot be found");

        TrackingRoot root(level);
        ChunkGenerateContext context(level, *chunk);
        feature->setRoot(&root);
        feature->apply(context);
        feature->setRoot(nullptr);
        const std::vector<Vector3i> changed = root.mChanged;
        root.applySubChunkUpdate();
        broadcastChanged(*api, level, changed);
        return JS_NewBool(ctx, !changed.empty());
    }
}

bool ScriptApi::readVolume(JSValueConst volume, std::vector<Vector3i> &locations) {
    JSContext *ctx = mContext;
    locations.clear();
    if (!JS_IsObject(volume)) {
        JS_ThrowTypeError(ctx, "Native type conversion failed.");
        return false;
    }
    JSAtom atom = JS_NewAtom(ctx, "getBlockLocationIterator");
    JSValue iterator = JS_Invoke(ctx, volume, atom, 0, nullptr);
    JS_FreeAtom(ctx, atom);
    if (JS_IsException(iterator))
        return false;
    JSAtom next = JS_NewAtom(ctx, "next");
    bool ok = true;
    while (true) {
        JSValue step = JS_Invoke(ctx, iterator, next, 0, nullptr);
        if (JS_IsException(step)) {
            ok = false;
            break;
        }
        JSValue done = JS_GetPropertyStr(ctx, step, "done");
        const bool finished = JS_ToBool(ctx, done) == 1;
        JS_FreeValue(ctx, done);
        if (finished) {
            JS_FreeValue(ctx, step);
            break;
        }
        JSValue value = JS_GetPropertyStr(ctx, step, "value");
        Vector3i location;
        if (readBlockLocation(ctx, value, location))
            locations.push_back(location);
        JS_FreeValue(ctx, value);
        JS_FreeValue(ctx, step);
    }
    JS_FreeAtom(ctx, next);
    JS_FreeValue(ctx, iterator);
    return ok;
}

JSValue ScriptApi::makeListVolume(const std::vector<Vector3i> &locations) {
    JSValue array = JS_NewArray(mContext);
    uint32_t index = 0;
    for (const Vector3i &location: locations)
        JS_SetPropertyUint32(mContext, array, index++,
                             vector3(mContext, (float) location.x, (float) location.y, (float) location.z));
    if (!JS_IsFunction(mContext, mListVolume))
        return array;
    JSValue volume = JS_CallConstructor(mContext, mListVolume, 1, &array);
    JS_FreeValue(mContext, array);
    return volume;
}

void ScriptApi::_bindDimensionSurface() {
    JSValue dimension = prototypeOf("Dimension");
    property(dimension, "Dimension", "heightRange", dimensionGetHeightRange);
    property(dimension, "Dimension", "localizationKey", dimensionGetLocalizationKey);
    method(dimension, "Dimension", "getBlockFromRay", dimensionGetBlockFromRay, 3);
    method(dimension, "Dimension", "getEntitiesFromRay", dimensionGetEntitiesFromRay, 3);
    methodMagic(dimension, "Dimension", "getBlockAbove", dimensionGetBlockVertical, 2, 0);
    methodMagic(dimension, "Dimension", "getBlockBelow", dimensionGetBlockVertical, 2, 1);
    methodMagic(dimension, "Dimension", "getLightLevel", dimensionLight, 1, 0);
    methodMagic(dimension, "Dimension", "getSkyLightLevel", dimensionLight, 1, 1);
    method(dimension, "Dimension", "getTopmostBlock", dimensionGetTopmostBlock, 2);
    method(dimension, "Dimension", "isChunkLoaded", dimensionIsChunkLoaded, 1);
    method(dimension, "Dimension", "setBlockPermutation", dimensionSetBlockPermutation, 2);
    method(dimension, "Dimension", "getEntitiesAtBlockLocation", dimensionGetEntitiesAtBlockLocation, 1);
    method(dimension, "Dimension", "getBlocks", dimensionGetBlocks, 3);
    method(dimension, "Dimension", "containsBlock", dimensionContainsBlock, 3);
    method(dimension, "Dimension", "fillBlocks", dimensionFillBlocks, 3);
    method(dimension, "Dimension", "cloneBlocks", dimensionCloneBlocks, 5);
    method(dimension, "Dimension", "createExplosion", dimensionCreateExplosion, 3);
    method(dimension, "Dimension", "setWeather", dimensionSetWeather, 2);
    method(dimension, "Dimension", "spawnXp", dimensionSpawnXp, 2);
    method(dimension, "Dimension", "getBiome", dimensionGetBiome, 1);
    method(dimension, "Dimension", "containsBiomes", dimensionContainsBiomes, 3);
    method(dimension, "Dimension", "calculateClosestBiomeFromSeed", dimensionClosestBiome, 3);
    method(dimension, "Dimension", "placeFeature", dimensionPlaceFeature, 3);
    method(dimension, "Dimension", "placeFeatureRule", dimensionPlaceFeatureRule, 2);

    if (mSurface.has("BiomeType")) {
        JSValue biome = defineClass("BiomeType");
        property(biome, "BiomeType", "id", biomeGetId);
        method(biome, "BiomeType", "getTags", biomeGetTags, 0);
        method(biome, "BiomeType", "hasTags", biomeHasTags, 1);
    }
    if (mSurface.has("BiomeTypes")) {
        defineClass("BiomeTypes");
        JSValue types = constructorOf("BiomeTypes");
        method(types, "BiomeTypes", "static get", biomeTypesGet, 1);
        method(types, "BiomeTypes", "static getAll", biomeTypesGetAll, 0);
    }
}
