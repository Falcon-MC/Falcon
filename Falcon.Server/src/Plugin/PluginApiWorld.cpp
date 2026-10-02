#include "Plugin/PluginServerApi.h"

#include "Actor/ServerActor.h"
#include "Block/BlockPaletteRegistry.h"
#include "Command/SetBlockCommand.h"
#include "Core/Json/Json.h"
#include "Level/Dimension.h"
#include "Level/Explosion.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Plugin/PluginApiHelpers.h"
#include "Protocol/Types/ItemStack.h"

#include <memory>
#include <string>
#include <vector>

using namespace PluginApiHelpers;

namespace {
    Vector3f toVector3f(const FalconVec3 &value) {
        return Vector3f((float) value.x, (float) value.y, (float) value.z);
    }

    Vector3i toVector3i(const FalconBlockPos &value) {
        return Vector3i(value.x, value.y, value.z);
    }

    std::string withNamespace(const std::string &identifier) {
        if (identifier.find(':') == std::string::npos)
            return "minecraft:" + identifier;
        return identifier;
    }

    bool isLoaded(Level &value, int32_t x, int32_t z) {
        return value.isChunkResident(x >> 4, z >> 4);
    }

    bool isInside(Level &value, const FalconBlockPos &position) {
        return position.y >= value.getMinY() && position.y <= value.getMaxY() &&
               isLoaded(value, position.x, position.z);
    }

    std::string stateValueToJson(const Tag &value) {
        switch (value.getType()) {
            case Tag::Type::Byte:
                return value.asByte() != 0 ? "true" : "false";
            case Tag::Type::Int:
                return std::to_string(value.asInt());
            case Tag::Type::String:
                return "\"" + json::escape(value.asString()) + "\"";
            default:
                return "null";
        }
    }

    bool applyStateValue(Tag &states, const std::string &key, const json::Value &value) {
        Tag *current = states.get(key);
        if (current == nullptr)
            return false;

        switch (current->getType()) {
            case Tag::Type::Byte:
                if (value.mType == json::Value::Type::Boolean) {
                    states.put(key, Tag::ofByte(value.mBoolean ? 1 : 0));
                    return true;
                }
                if (!value.isNumber())
                    return false;
                states.put(key, Tag::ofByte((int8_t) value.integer()));
                return true;
            case Tag::Type::Int:
                if (!value.isNumber())
                    return false;
                states.put(key, Tag::ofInt(value.integer()));
                return true;
            case Tag::Type::String:
                if (!value.isString())
                    return false;
                states.put(key, Tag::ofString(value.mString));
                return true;
            default:
                return false;
        }
    }

    bool isKnownPermutation(const std::string &identifier, const Tag &states) {
        const std::vector<Tag> *permutations = BlockPaletteRegistry::getInstance().getPermutations(identifier);
        if (permutations == nullptr)
            return false;

        for (const Tag &permutation: *permutations) {
            if (permutation == states)
                return true;
        }

        return false;
    }

    bool resolveState(const std::string &name, const char *statesJson, BlockState &out) {
        if (!SetBlockCommand::resolveBlock(name, out))
            return false;

        if (statesJson == nullptr)
            return true;

        const std::unique_ptr<json::Value> parsed = json::parse(statesJson);
        if (parsed == nullptr || !parsed->isObject())
            return false;

        for (const auto &entry: parsed->mObject) {
            if (!applyStateValue(out.mStates, entry.first, *entry.second))
                return false;
        }

        return isKnownPermutation(out.mName, out.mStates);
    }

    FalconLevel *serverLevel(FalconDimension dimension) {
        const DimensionType type = Dimension::fromId((int32_t) dimension);
        if ((uint32_t) Dimension::toId(type) != dimension)
            return nullptr;

        for (Level *value: owner().getLevels()) {
            if (value->getDimensionType() == type)
                return toHandle(value);
        }

        return nullptr;
    }

    FalconDimension levelDimension(FalconLevel *handle) {
        return (FalconDimension) Dimension::toId(level(handle)->getDimensionType());
    }

    const char *levelName(FalconLevel *handle) {
        return hold(level(handle)->getName());
    }

    const char *levelGetBlock(FalconLevel *handle, FalconBlockPos position) {
        Level *value = level(handle);
        if (!isInside(*value, position))
            return hold("minecraft:air");

        return hold(value->getBlockState(position.x, position.y, position.z).mName);
    }

    const char *levelGetBlockStates(FalconLevel *handle, FalconBlockPos position) {
        Level *value = level(handle);
        if (!isInside(*value, position))
            return hold("{}");

        const BlockState state = value->getBlockState(position.x, position.y, position.z);
        const std::vector<std::string> &keys = state.mStates.getKeys();
        const std::vector<Tag> &values = state.mStates.getValues();

        std::string result = "{";
        for (size_t index = 0; index < keys.size() && index < values.size(); ++index) {
            if (index > 0)
                result += ",";
            result += "\"" + json::escape(keys[index]) + "\":" + stateValueToJson(values[index]);
        }
        result += "}";
        return hold(result);
    }

    int levelSetBlock(FalconLevel *handle, FalconBlockPos position, const char *name, const char *statesJson) {
        if (name == nullptr)
            return 0;

        Level *value = level(handle);
        if (!isInside(*value, position))
            return 0;

        BlockState state;
        if (!resolveState(withNamespace(name), statesJson, state))
            return 0;

        return SetBlockCommand::placeBlock(owner(), *value, toVector3i(position), state, "replace") ? 1 : 0;
    }

    int levelBreakBlock(FalconLevel *handle, FalconBlockPos position, int dropItems) {
        Level *value = level(handle);
        if (!isInside(*value, position))
            return 0;

        const BlockState existing = value->getBlockState(position.x, position.y, position.z);
        if (existing.mName == "minecraft:air")
            return 0;

        BlockActionHandler::destroyBlock(owner(), *value, toVector3i(position), existing, dropItems != 0,
                                         ItemStack::air());
        return 1;
    }

    int levelIsChunkLoaded(FalconLevel *handle, int32_t chunkX, int32_t chunkZ) {
        return level(handle)->isChunkResident(chunkX, chunkZ) ? 1 : 0;
    }

    int32_t levelHighestBlockY(FalconLevel *handle, int32_t x, int32_t z) {
        Level *value = level(handle);
        if (!isLoaded(*value, x, z))
            return value->getMinY() - 1;

        for (int32_t y = value->getMaxY(); y >= value->getMinY(); --y) {
            const BlockState *state = value->peekBlockPtr(x, y, z);
            if (state != nullptr && state->mName != "minecraft:air")
                return y;
        }

        return value->getMinY() - 1;
    }

    int64_t levelTime(FalconLevel *handle) {
        (void) handle;
        return owner().getLevel().getTime();
    }

    void levelSetTime(FalconLevel *handle, int64_t time) {
        (void) handle;
        owner().getLevel().setTime(time);
        owner().broadcastWorldTime();
    }

    int levelIsRaining(FalconLevel *handle) {
        (void) handle;
        return owner().getLevel().isRaining() ? 1 : 0;
    }

    void levelSetRaining(FalconLevel *handle, int raining) {
        (void) handle;
        owner().setRaining(raining != 0);
    }

    int levelIsThundering(FalconLevel *handle) {
        (void) handle;
        return owner().getLevel().isThundering() ? 1 : 0;
    }

    void levelSetThundering(FalconLevel *handle, int thundering) {
        (void) handle;
        owner().setThundering(thundering != 0);
    }

    FalconVec3 levelSpawnPosition(FalconLevel *handle) {
        const Vector3i spawn = level(handle)->getSpawnPosition();
        return FalconVec3{(double) spawn.x, (double) spawn.y, (double) spawn.z};
    }

    FalconEntity *levelSpawnEntity(FalconLevel *handle, const char *identifier, FalconVec3 position) {
        if (identifier == nullptr)
            return nullptr;

        ServerActor *spawned = owner().spawnActor(*level(handle), withNamespace(identifier), toVector3f(position));
        if (spawned == nullptr)
            return nullptr;

        return toHandle(static_cast<Actor *>(spawned));
    }

    void levelDropItem(FalconLevel *handle, FalconVec3 position, FalconItem *dropped) {
        if (dropped == nullptr)
            return;

        const ItemStack copy = *item(dropped);
        owner().dropItem(*level(handle), toVector3f(position), copy, ItemActorHandler::randomDropMotion(),
                         ItemActorHandler::DROP_PICKUP_DELAY);
    }

    void levelStrikeLightning(FalconLevel *handle, FalconVec3 position) {
        owner().strikeLightning(*level(handle), toVector3f(position));
    }

    void levelCreateExplosion(FalconLevel *handle, FalconVec3 position, float power, int breakBlocks) {
        if (power <= 0.0f)
            return;

        Explosion explosion(owner(), *level(handle), toVector3f(position), (double) power, nullptr, false);
        if (breakBlocks != 0)
            explosion.explode();
        else
            explosion.explodeWithoutBlocks();
    }
}

void PluginServerApi::fillWorld(FalconServerApi &api) {
    api.serverLevel = &serverLevel;
    api.levelDimension = &levelDimension;
    api.levelName = &levelName;
    api.levelGetBlock = &levelGetBlock;
    api.levelGetBlockStates = &levelGetBlockStates;
    api.levelSetBlock = &levelSetBlock;
    api.levelBreakBlock = &levelBreakBlock;
    api.levelIsChunkLoaded = &levelIsChunkLoaded;
    api.levelHighestBlockY = &levelHighestBlockY;
    api.levelTime = &levelTime;
    api.levelSetTime = &levelSetTime;
    api.levelIsRaining = &levelIsRaining;
    api.levelSetRaining = &levelSetRaining;
    api.levelIsThundering = &levelIsThundering;
    api.levelSetThundering = &levelSetThundering;
    api.levelSpawnPosition = &levelSpawnPosition;
    api.levelSpawnEntity = &levelSpawnEntity;
    api.levelDropItem = &levelDropItem;
    api.levelStrikeLightning = &levelStrikeLightning;
    api.levelCreateExplosion = &levelCreateExplosion;
}
