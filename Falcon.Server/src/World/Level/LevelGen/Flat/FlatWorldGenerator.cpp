#include "World/Level/LevelGen/Flat/FlatWorldGenerator.h"

#include "Core/Json/Json.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "Protocol/BlockStateHasher.h"

#include <memory>

namespace {

    const char *const AIR = "minecraft:air";
    const char *const POST_1_18 = "version.post_1_18";

    // worlds here always span the post-1.18 height: legacy stacks get the 64 air layers the game inserts to keep
    // their bedrock at y=0
    const int32_t LEGACY_AIR_LAYERS = 64;

    // what the game writes for a post-1.18 world when FlatWorldLayers is missing or unreadable
    const char *const DEFAULT_LAYERS = R"({
        "biome_id" : 1,
        "world_version" : "version.post_1_18",
        "block_layers" : [
            { "block_name" : "minecraft:bedrock", "count" : 1 },
            { "block_name" : "minecraft:dirt", "count" : 2 },
            { "block_name" : "minecraft:grass_block", "count" : 1 }
        ],
        "encoding_version" : 6,
        "structure_options" : null
    })";

    BlockState defaultState(const std::string &name) {
        BlockPaletteRegistry &registry = BlockPaletteRegistry::getInstance();
        registry.initialize();

        const Tag *states = registry.getDefaultStates(name);
        return states == nullptr ? BlockState(name) : BlockState(name, *states);
    }

    // the game resolves an unknown block name to info_update instead of rejecting the layer
    BlockState layerBlock(const json::Value &layer) {
        BlockPaletteRegistry &registry = BlockPaletteRegistry::getInstance();
        registry.initialize();

        const json::Value *name = layer.get("block_name");
        const std::string identifier = name == nullptr ? std::string() : name->string();
        if (registry.getDefaultStates(identifier) == nullptr)
            return defaultState("minecraft:info_update");

        return defaultState(identifier);
    }

    bool hasLayerFields(const json::Value &layer) {
        return layer.get("block_name") != nullptr && layer.get("count") != nullptr;
    }

    int32_t layerCount(const json::Value &layer) {
        return layer.get("count")->integer(0);
    }

}

FlatWorldGenerator::FlatWorldGenerator(int64_t seed, const std::string &layers)
        : mSeed(seed), mAirHash(BlockStateHasher::hash(AIR)), mBiomeId(0), mSpawnY(LevelChunk::MIN_Y + 1),
          mStone(defaultState("minecraft:stone")), mCobblestone(defaultState("minecraft:cobblestone")) {
    if (!_parse(layers))
        _parse(DEFAULT_LAYERS);

    const int32_t worldHeight = LevelChunk::MAX_Y - LevelChunk::MIN_Y + 1;
    for (const Layer &layer: mLayers) {
        for (int32_t i = 0; i < layer.mCount && (int32_t) mColumn.size() < worldHeight; ++i) {
            mColumn.push_back(layer.mBlock);
            if (layer.mBlock.mName != AIR)
                mSpawnY = LevelChunk::MIN_Y + (int32_t) mColumn.size();
        }
    }
}

bool FlatWorldGenerator::_parse(const std::string &source) {
    mLayers.clear();

    const std::unique_ptr<json::Value> root = json::parse(source);
    const json::Value empty;
    const json::Value &settings = root == nullptr ? empty : *root;

    const json::Value *encoding = settings.get("encoding_version");
    const int32_t version = encoding == nullptr ? 0 : encoding->integer(0);
    const json::Value *biome = settings.get("biome_id");
    mBiomeId = biome == nullptr ? 0 : (uint16_t) biome->integer(0);

    // encodings 0 to 3 store numeric legacy block ids: no table for them here
    if (version <= 3)
        return false;

    if (version > 6)
        return true;

    const json::Value *layers = settings.get("block_layers");
    if (layers == nullptr || layers->mType != json::Value::Type::Array)
        return false;

    const BlockState air = defaultState(AIR);
    bool prependAir = version != 6;
    if (version == 6) {
        const json::Value *worldVersion = settings.get("world_version");
        if (worldVersion == nullptr || worldVersion->string() != POST_1_18) {
            const json::Value *first = layers->mArray.empty() ? nullptr : layers->mArray.front().get();
            prependAir = first == nullptr || !hasLayerFields(*first) || layerBlock(*first).mName != AIR ||
                         layerCount(*first) != LEGACY_AIR_LAYERS;
        }
    }

    if (prependAir)
        mLayers.push_back({air, LEGACY_AIR_LAYERS});

    size_t legacyAirEntries = 0;
    for (const std::unique_ptr<json::Value> &layer: layers->mArray) {
        if (version == 4) {
            const json::Value *blockId = layer->get("block_id");
            if (blockId != nullptr && blockId->integer(0) == 0 && layer->get("block_name") == nullptr) {
                ++legacyAirEntries;
                mLayers.push_back({defaultState("minecraft:info_update"), 1});
                continue;
            }
        }

        if (!hasLayerFields(*layer)) {
            mLayers.clear();
            // a broken layer in an encoding 6 stack: empty stack, not the default
            return version == 6;
        }

        mLayers.push_back({layerBlock(*layer), layerCount(*layer)});
    }

    if (version == 4 && legacyAirEntries != 0 && legacyAirEntries == layers->mArray.size()) {
        mLayers.clear();
        return false;
    }

    return true;
}

void FlatWorldGenerator::generate(LevelChunk &chunk) const {
    if (mLayers.empty()) {
        _generateEmpty(chunk);
    } else {
        for (int32_t index = 0; index < (int32_t) mColumn.size(); ++index) {
            const BlockState &block = mColumn[index];
            if (block.mName == AIR)
                continue;

            for (int32_t x = 0; x < 16; ++x) {
                for (int32_t z = 0; z < 16; ++z)
                    chunk.setBlock(x, LevelChunk::MIN_Y + index, z, block);
            }
        }
    }

    // same column everywhere: its top is known without scanning
    const int32_t height = mLayers.empty() ? LevelChunk::MIN_Y : mSpawnY - 1;
    for (int32_t x = 0; x < 16; ++x) {
        for (int32_t z = 0; z < 16; ++z)
            chunk.setHeight(x, z, height);
    }

    chunk.setBiome((uint32_t) mBiomeId);
    chunk.clearDirty();
}

void FlatWorldGenerator::_generateEmpty(LevelChunk &chunk) const {
    // no layers: the game builds a 33x33 stone platform at the world bottom around the origin, cobblestone at 0,0
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();
    if (chunkX < -1 || chunkX > 1 || chunkZ < -1 || chunkZ > 1)
        return;

    const int32_t maxX = chunkX == 1 ? 1 : 16;
    const int32_t maxZ = chunkZ == 1 ? 1 : 16;
    for (int32_t x = 0; x < maxX; ++x) {
        for (int32_t z = 0; z < maxZ; ++z)
            chunk.setBlock(x, LevelChunk::MIN_Y, z, mStone);
    }

    if (chunkX == 0 && chunkZ == 0)
        chunk.setBlock(0, LevelChunk::MIN_Y, 0, mCobblestone);
}

void FlatWorldGenerator::decorate(Level &, LevelChunk &, std::vector<GeneratedBlockChange> *) const {
}

int32_t FlatWorldGenerator::pickBiome(int32_t, int32_t, int32_t) const {
    return mBiomeId;
}
