#include "World/Level/LevelGen/Structure/VillageStructure.h"

#include "Core/Color/DyeColor.h"
#include "World/Level/Block/Actor/BedBlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Overworld/Terrain/TerrainBeard.h"
#include "World/Level/LevelGen/Structure/StructureBlockTable.h"
#include "World/Level/LevelGen/Structure/StructureConnections.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureProcessors.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"
#include "World/Level/LevelGen/Structure/StructureTerrain.h"

#include <deque>
#include <mutex>
#include <string>

namespace {
    const int32_t MAX_DEPTH = 6;
    const int32_t SEA_LEVEL = 63;
    const float ABANDONED_CHANCE = 0.02f;

    // how many chunks a village can grow from its start; a town fits well inside this
    const int32_t REACH = 6;

    // a chunk asks for every start within REACH and a village reaches about a hundred chunks, so the plans of a few
    // dozen starts cover the chunks being generated at once
    const size_t CACHED_PLANS = 64;

    const char *ABANDONED_MOBS = "minecraft:abandoned_mobs";
    const char *ABANDONED_GOLEMS = "minecraft:abandoned_golems";

    void rotate(int32_t quarterTurns, int32_t &x, int32_t &z) {
        for (int32_t i = 0; i < quarterTurns; ++i) {
            const int32_t oldX = x;
            x = -z;
            z = oldX;
        }
    }

    const char *styleOf(int32_t biomeId) {
        switch (biomeId) {
            case BiomeIds::DESERT:
                return "desert";
            case BiomeIds::ICE_PLAINS:
            case BiomeIds::COLD_TAIGA:
                return "snowy";
            case BiomeIds::SAVANNA:
                return "savanna";
            case BiomeIds::TAIGA:
                return "taiga";
            default:
                return "plains";
        }
    }

    std::vector<std::string> processorNames(const JigsawElementData &element) {
        std::vector<std::string> names;
        std::string current;
        for (const char *c = element.mProcessors; c != nullptr && *c != '\0'; ++c) {
            if (*c != ',') {
                current += *c;
                continue;
            }
            names.push_back(current);
            current.clear();
        }
        if (!current.empty())
            names.push_back(current);
        return names;
    }

    bool hasProcessor(const std::vector<std::string> &names, const char *name) {
        for (const std::string &candidate : names) {
            if (candidate == name)
                return true;
        }
        return false;
    }

    // the lists of an element run one after the other, as one list
    std::vector<const StructureProcessors::Rule *> rulesOf(const std::vector<std::string> &names) {
        std::vector<const StructureProcessors::Rule *> rules;
        for (const std::string &name : names) {
            for (const StructureProcessors::Rule &rule : StructureProcessors::get(name))
                rules.push_back(&rule);
        }
        return rules;
    }

    // a final state is written the way the editor shows a block: a name, then its properties in brackets
    bool parseState(const std::string &text, StructureTemplate::PaletteEntry &out) {
        const size_t open = text.find('[');
        out.mName = text.substr(0, open);
        out.mProperties.clear();
        if (open == std::string::npos)
            return true;

        const size_t close = text.find(']', open);
        if (close == std::string::npos)
            return false;

        size_t start = open + 1;
        while (start < close) {
            size_t end = text.find(',', start);
            if (end == std::string::npos || end > close)
                end = close;
            const size_t equals = text.find('=', start);
            if (equals == std::string::npos || equals > end)
                return false;
            out.mProperties[text.substr(start, equals - start)] = text.substr(equals + 1, end - equals - 1);
            start = end + 1;
        }
        return true;
    }

    // a data block reads "identifier" or "identifier:event"; the mob is the identifier, its two first fields
    std::string mobOf(const std::string &metadata) {
        const size_t first = metadata.find(':');
        if (first == std::string::npos)
            return metadata;

        return metadata.substr(0, metadata.find(':', first + 1));
    }

    bool insideChunk(int32_t x, int32_t z, int32_t chunkX, int32_t chunkZ) {
        return (x >> 4) == chunkX && (z >> 4) == chunkZ;
    }

    bool touchesChunk(const JigsawBox &box, int32_t chunkX, int32_t chunkZ) {
        return box.mMaxX >= chunkX * 16 && box.mMinX <= chunkX * 16 + 15 && box.mMaxZ >= chunkZ * 16
               && box.mMinZ <= chunkZ * 16 + 15;
    }

    Tag generatedMob(const std::string &identifier, const VillageStructure::Position &position) {
        Tag location = Tag::ofList(Tag::Type::Float);
        location.addToList(Tag::ofFloat((float) std::get<0>(position) + 0.5f));
        location.addToList(Tag::ofFloat((float) std::get<1>(position)));
        location.addToList(Tag::ofFloat((float) std::get<2>(position) + 0.5f));

        Tag mob = Tag::ofCompound();
        mob.putString("identifier", identifier);
        mob.put("Pos", location);
        return mob;
    }

    // "minecraft:chests/village/village_plains_house" names the table loot_tables/chests/village/village_plains_house
    std::string lootTablePath(const std::string &name) {
        const size_t colon = name.find(':');
        return "loot_tables/" + (colon == std::string::npos ? name : name.substr(colon + 1)) + ".json";
    }

    /**
     * One piece in one chunk, as the jigsaw element places it (sub_145C22000). Every block is clipped to the chunk.
     * The jigsaw blocks go first, each turned into its final state (sub_145C24D90); then the template, block by block
     * in the order of the file (sub_14775E430). The template draws from a random seeded with the seed of the chunk's
     * post process random: the rules of the element as they test blocks, and a chest with no seed of its own as it is
     * placed. Each jigsaw block draws from a random seeded the same way again (sub_1477605F0).
     */
    class PiecePlacer {
    public:
        PiecePlacer(const JigsawPiece &piece, int32_t chunkX, int32_t chunkZ, uint32_t seed, StructureTerrain &terrain,
                    VillageStructure::ChunkShare &share)
            : mPiece(piece), mChunkX(chunkX), mChunkZ(chunkZ), mSeed(seed), mTerrain(terrain), mShare(share) {
            mProcessors = processorNames(*piece.mElement);
            mRules = rulesOf(mProcessors);
        }

        void place(const StructureTemplate &shape) {
            for (const StructureTemplate::BlockEntry &entry : shape.getBlocks()) {
                if (shape.getPalette()[(size_t) entry.mPaletteIndex].mName == "minecraft:jigsaw")
                    _placeJigsaw(entry);
            }

            std::mt19937 random(mSeed);
            for (const StructureTemplate::BlockEntry &entry : shape.getBlocks())
                _placeBlock(shape.getPalette()[(size_t) entry.mPaletteIndex], entry, random);
        }

    private:
        bool _position(const StructureTemplate::BlockEntry &entry, VillageStructure::Position &out) {
            int32_t x = entry.x;
            int32_t z = entry.z;
            rotate(mPiece.mQuarterTurns, x, z);
            x += mPiece.x;
            z += mPiece.z;
            if (!insideChunk(x, z, mChunkX, mChunkZ))
                return false;

            // a ground-hugging piece follows the top of each column, water included
            const int32_t y = mPiece.mElement->mTerrainMatching ? mTerrain.getTopHeight(x, z) - 1 + entry.y
                                                                : mPiece.y + entry.y;
            if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
                return false;

            out = {x, y, z};
            return true;
        }

        BlockState _world(const VillageStructure::Position &position) {
            const auto placed = mShare.mBlocks.find(position);
            if (placed != mShare.mBlocks.end())
                return placed->second;

            return mTerrain.getBlock(std::get<0>(position), std::get<1>(position), std::get<2>(position));
        }

        void _placeJigsaw(const StructureTemplate::BlockEntry &entry) {
            VillageStructure::Position position;
            if (!_position(entry, position))
                return;

            StructureTemplate::PaletteEntry finalState;
            BlockState block;
            if (!parseState(entry.mData.getString("final_state", "minecraft:air"), finalState)
                || !StructureBlockTable::translate(finalState, mPiece.mQuarterTurns, block))
                return;

            std::mt19937 random(mSeed);
            mShare.mBlocks[position] = StructureProcessors::apply(mRules, block, _world(position), random);
        }

        void _placeBlock(const StructureTemplate::PaletteEntry &palette, const StructureTemplate::BlockEntry &entry,
                         std::mt19937 &random) {
            // the element leaves the jigsaw blocks to their final state and the data blocks to the mobs they name
            if (palette.mName == "minecraft:jigsaw")
                return;

            VillageStructure::Position position;
            if (!_position(entry, position))
                return;

            if (palette.mName == "minecraft:structure_block") {
                _spawn(entry.mData.getString("metadata", std::string()), position);
                return;
            }

            BlockState block;
            if (palette.mName == "minecraft:structure_void"
                || !StructureBlockTable::translate(palette, mPiece.mQuarterTurns, block))
                return;

            block = StructureProcessors::apply(mRules, block, _world(position), random);
            mShare.mBlocks[position] = block;
            if (block.mName == "minecraft:bed") {
                const uint8_t color = DyeColor::fromIdentifier(palette.mName, "_bed");
                if (color != DyeColor::INVALID)
                    mShare.mBeds.push_back({position, color});
            }

            if (entry.mData.getString("id", std::string()) != "minecraft:chest" || block.mName != "minecraft:chest")
                return;

            VillageStructure::Chest chest;
            chest.mPosition = position;
            chest.mLootTable = lootTablePath(entry.mData.getString("LootTable", std::string()));
            chest.mLootSeed = entry.mData.contains("LootTableSeed") ? entry.mData.getInt("LootTableSeed", 0)
                                                                    : FeatureRandom::nextInt(random);
            if (!entry.mData.contains("LootTable"))
                chest.mLootTable.clear();
            mShare.mChests.push_back(chest);
        }

        void _spawn(const std::string &metadata, const VillageStructure::Position &position) {
            std::string mob = mobOf(metadata);
            if (mob.empty())
                return;

            // an abandoned village keeps no golem and has its villagers turned (sub_14CD6B070)
            if (mob == "minecraft:iron_golem" && hasProcessor(mProcessors, ABANDONED_GOLEMS))
                return;
            if (mob == "minecraft:villager_v2" && hasProcessor(mProcessors, ABANDONED_MOBS))
                mob = "minecraft:zombie_villager_v2";

            mShare.mMobs.push_back({position, mob});
        }

        const JigsawPiece &mPiece;
        int32_t mChunkX;
        int32_t mChunkZ;
        uint32_t mSeed;
        StructureTerrain &mTerrain;
        VillageStructure::ChunkShare &mShare;
        std::vector<std::string> mProcessors;
        std::vector<const StructureProcessors::Rule *> mRules;
    };
}

std::vector<JigsawPiece> VillageStructure::plan(const ChunkGenerator &generator, int32_t startX, int32_t startZ) {
    const int32_t cornerX = startX * 16;
    const int32_t cornerZ = startZ * 16;
    const int32_t surface = generator.getPreliminarySurfaceLevel(startX * 4, startZ * 4);
    const std::string style = styleOf(generator.pickBiome(cornerX + 8, surface, cornerZ + 8));

    const StructureFeature *village = StructureFeature::get("village");
    if (village == nullptr)
        return {};

    std::mt19937 random = village->createStartRandom(generator.getSeed(), startX, startZ);
    const int32_t quarterTurns = FeatureRandom::nextInt(random, 4);
    const bool abandoned = FeatureRandom::nextFloat(random) < ABANDONED_CHANCE;

    const JigsawPools::Pool *centres =
            JigsawPools::get("minecraft:village/" + std::string(abandoned ? "abandoned/" : "") + style + "/town_centers");
    if (centres == nullptr || centres->mSlots.empty())
        return {};

    const JigsawElementData *start = centres->mSlots[(size_t) FeatureRandom::nextInt(random, (int32_t) centres->mSlots.size())];
    if (start == nullptr)
        return {};

    JigsawAssembler assembler(generator, random, MAX_DEPTH, SEA_LEVEL);
    return assembler.assemble(*start, cornerX, surface, cornerZ, quarterTurns);
}

std::shared_ptr<const std::vector<JigsawPiece>> VillageStructure::cachedPlan(const ChunkGenerator &generator,
                                                                             int32_t startX, int32_t startZ) {
    using Key = std::tuple<const ChunkGenerator *, int64_t, int32_t, int32_t>;
    static std::mutex mutex;
    static std::map<Key, std::shared_ptr<const std::vector<JigsawPiece>>> plans;
    static std::deque<Key> order;

    const StructureFeature *village = StructureFeature::get("village");
    if (village == nullptr || !village->isStartChunk(generator, startX, startZ))
        return nullptr;

    const Key key = {&generator, generator.getSeed(), startX, startZ};
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = plans.find(key);
        if (found != plans.end())
            return found->second;
    }

    // planned outside the lock: chunk workers planning other villages must not wait on this one
    auto pieces = std::make_shared<const std::vector<JigsawPiece>>(plan(generator, startX, startZ));
    std::lock_guard<std::mutex> lock(mutex);
    const auto inserted = plans.emplace(key, pieces);
    if (!inserted.second)
        return inserted.first->second;

    order.push_back(key);
    if (order.size() > CACHED_PLANS) {
        plans.erase(order.front());
        order.pop_front();
    }
    return pieces;
}

VillageStructure::ChunkShare VillageStructure::placeInChunk(const ChunkGenerator &generator,
                                                            const std::vector<JigsawPiece> &pieces, int32_t chunkX,
                                                            int32_t chunkZ, StructureTerrain &terrain) {
    ChunkShare share;
    const uint32_t seed = StructureFeature::getPostProcessSeed(generator.getSeed(), chunkX, chunkZ);
    for (const JigsawPiece &piece : pieces) {
        if (piece.mElement->mTemplate == nullptr || !touchesChunk(piece.mBox, chunkX, chunkZ))
            continue;

        const std::shared_ptr<const StructureTemplate> shape = StructureTemplate::load(piece.mElement->mTemplate);
        if (shape == nullptr)
            continue;

        PiecePlacer(piece, chunkX, chunkZ, seed, terrain, share).place(*shape);
    }
    return share;
}

void VillageStructure::addBeard(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ,
                                TerrainBeard &beard) {
    for (int32_t startX = chunkX - REACH; startX <= chunkX + REACH; ++startX) {
        for (int32_t startZ = chunkZ - REACH; startZ <= chunkZ + REACH; ++startZ) {
            const std::shared_ptr<const std::vector<JigsawPiece>> pieces = cachedPlan(generator, startX, startZ);
            if (pieces == nullptr)
                continue;

            // the streets follow the ground, only the rigid pieces bend the terrain to them
            for (const JigsawPiece &piece : *pieces) {
                if (piece.mElement->mTerrainMatching)
                    continue;

                const JigsawBox &box = piece.mBox;
                beard.addPiece(chunkX, chunkZ,
                               {box.mMinX, box.mMinY, box.mMinZ, box.mMaxX, box.mMaxY, box.mMaxZ,
                                TerrainBeard::Adaptation::BeardThin, piece.mGroundLevelDelta});
            }
        }
    }
}

void VillageStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    for (int32_t startX = chunk.getX() - REACH; startX <= chunk.getX() + REACH; ++startX) {
        for (int32_t startZ = chunk.getZ() - REACH; startZ <= chunk.getZ() + REACH; ++startZ) {
            const std::shared_ptr<const std::vector<JigsawPiece>> pieces = cachedPlan(generator, startX, startZ);
            if (pieces == nullptr)
                continue;

            bool reaches = false;
            for (const JigsawPiece &piece : *pieces)
                reaches = reaches || touchesChunk(piece.mBox, chunk.getX(), chunk.getZ());
            if (!reaches)
                continue;

            StructureTerrain terrain(generator);
            const ChunkShare own = placeInChunk(generator, *pieces, chunk.getX(), chunk.getZ(), terrain);

            // fences, panes and stairs connect across the chunk border, so the neighbours' shares are needed too
            std::map<Position, BlockState> around = own.mBlocks;
            for (int32_t dx = -1; dx <= 1; ++dx) {
                for (int32_t dz = -1; dz <= 1; ++dz) {
                    if (dx == 0 && dz == 0)
                        continue;
                    const ChunkShare next = placeInChunk(generator, *pieces, chunk.getX() + dx, chunk.getZ() + dz,
                                                         terrain);
                    around.insert(next.mBlocks.begin(), next.mBlocks.end());
                }
            }

            const auto neighbour = [&](int32_t x, int32_t y, int32_t z) -> BlockState {
                const auto found = around.find({x, y, z});
                return found != around.end() ? found->second : terrain.getBlock(x, y, z);
            };

            for (const auto &entry : own.mBlocks) {
                const int32_t x = std::get<0>(entry.first);
                const int32_t y = std::get<1>(entry.first);
                const int32_t z = std::get<2>(entry.first);
                chunk.setBlock(x & 15, y, z & 15, StructureConnections::shaped(entry.second, x, y, z, neighbour));
            }

            for (const Chest &chest : own.mChests) {
                ChestBlockActor &actor = level.getBlockActors().getOrCreate<ChestBlockActor>(
                        Vector3i(std::get<0>(chest.mPosition), std::get<1>(chest.mPosition),
                                 std::get<2>(chest.mPosition)));
                if (!chest.mLootTable.empty())
                    actor.setLootTable(chest.mLootTable, chest.mLootSeed);
            }

            for (const Bed &bed : own.mBeds) {
                level.getBlockActors()
                        .getOrCreate<BedBlockActor>(Vector3i(std::get<0>(bed.mPosition), std::get<1>(bed.mPosition),
                                                             std::get<2>(bed.mPosition)))
                        .setColor((int8_t) bed.mColor);
            }

            for (const Mob &mob : own.mMobs)
                level.addGeneratedEntity(generatedMob(mob.mIdentifier, mob.mPosition));
        }
    }
}
