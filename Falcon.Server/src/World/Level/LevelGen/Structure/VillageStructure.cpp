#include "World/Level/LevelGen/Structure/VillageStructure.h"

#include "Core/Color/DyeColor.h"
#include "World/Level/Block/Actor/BannerBlockActor.h"
#include "World/Level/Block/Actor/BedBlockActor.h"
#include "World/Level/Block/Actor/BellBlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/CauldronBlockActor.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/Actor/FlowerPotBlockActor.h"
#include "World/Level/Block/Actor/FurnaceBlockActor.h"
#include "World/Level/Block/Actor/SignBlockActor.h"
#include "World/Level/Block/BannerBlock.h"
#include "World/Level/Block/BellBlock.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/CauldronBlock.h"
#include "World/Level/Block/ContainerBlock.h"
#include "World/Level/Block/FlowerPotBlock.h"
#include "World/Level/Block/FurnaceBlock.h"
#include "World/Level/Block/SignBlock.h"
#include "World/Level/Block/WallBannerBlock.h"
#include "World/Level/Block/WallSignBlock.h"
#include "World/Level/Block/Inventory/FurnaceInventory.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Overworld/Feature/OverworldRulePlacer.h"
#include "World/Level/LevelGen/Overworld/Surface/SurfaceMaterialAttributes.h"
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

    // a village spans ~100 chunks: a few dozen plans cover the chunks generated at once
    const size_t CACHED_PLANS = 64;

    const char *ABANDONED_MOBS = "minecraft:abandoned_mobs";
    const char *ABANDONED_GOLEMS = "minecraft:abandoned_golems";

    const char *styleOf(int32_t biomeId) {
        static const char *STYLES[] = {"desert", "snowy", "savanna", "taiga"};
        const int32_t type = BiomeChunkGenDataRegistry::getVillageType(biomeId);
        return type >= 0 && type < 4 ? STYLES[type] : "plains";
    }

    struct ElementRules {
        std::vector<std::string> mProcessors;
        std::vector<const StructureProcessors::Rule *> mRules;
    };

    // read once per element: the pools are static
    const ElementRules &rulesOf(const JigsawElementData &element) {
        static std::mutex mutex;
        static std::map<const JigsawElementData *, std::unique_ptr<const ElementRules>> cache;
        const std::lock_guard<std::mutex> lock(mutex);
        std::unique_ptr<const ElementRules> &cached = cache[&element];
        if (cached != nullptr)
            return *cached;

        auto rules = std::make_unique<ElementRules>();
        std::string current;
        for (const char *c = element.mProcessors; c != nullptr && *c != '\0'; ++c) {
            if (*c != ',') {
                current += *c;
                continue;
            }
            rules->mProcessors.push_back(current);
            current.clear();
        }
        if (!current.empty())
            rules->mProcessors.push_back(current);

        for (const std::string &name : rules->mProcessors) {
            for (const StructureProcessors::Rule &rule : StructureProcessors::get(name))
                rules->mRules.push_back(&rule);
        }
        cached = std::move(rules);
        return *cached;
    }

    bool hasProcessor(const std::vector<std::string> &names, const char *name) {
        for (const std::string &candidate : names) {
            if (candidate == name)
                return true;
        }
        return false;
    }

    // a palette entry as placed; false when the template leaves the position alone
    struct TranslatedEntry {
        bool mPlaced = false;
        BlockState mBlock;
    };

    using TranslatedPalette = std::vector<TranslatedEntry>;

    // templates stay loaded (StructureTemplate::load): their address keys the cache
    std::shared_ptr<const TranslatedPalette> translatedPalette(const StructureTemplate &shape, int32_t quarterTurns) {
        static std::mutex mutex;
        static std::map<std::pair<const StructureTemplate *, int32_t>, std::shared_ptr<const TranslatedPalette>> cache;
        const std::pair<const StructureTemplate *, int32_t> key(&shape, quarterTurns);
        {
            const std::lock_guard<std::mutex> lock(mutex);
            const auto found = cache.find(key);
            if (found != cache.end())
                return found->second;
        }

        auto palette = std::make_shared<TranslatedPalette>();
        for (const StructureTemplate::PaletteEntry &entry : shape.getPalette()) {
            TranslatedEntry translated;
            // jigsaws and structure blocks are read from their data, never translated
            translated.mPlaced = entry.mName != "minecraft:jigsaw" && entry.mName != "minecraft:structure_block"
                                 && entry.mName != "minecraft:structure_void"
                                 && StructureBlockTable::translate(entry, quarterTurns, translated.mBlock);
            // workers only copy the entry: its lazy hash is filled before it is shared
            translated.mBlock.getHash();
            palette->push_back(std::move(translated));
        }

        const std::lock_guard<std::mutex> lock(mutex);
        return cache.emplace(key, std::move(palette)).first->second;
    }

    // name[key=value,...]
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

    // "identifier" or "identifier:event"
    void mobOf(const std::string &metadata, std::string &identifier, std::string &event) {
        const size_t first = metadata.find(':');
        const size_t second = first == std::string::npos ? std::string::npos : metadata.find(':', first + 1);
        identifier = metadata.substr(0, second);
        event = second == std::string::npos ? std::string() : metadata.substr(second + 1);
    }

    // the blocks of the village templates the game gives a block actor as it places them
    bool hasBlockActor(const std::string &identifier) {
        const Block *block = VanillaBlocks::fromIdentifier(identifier);
        return block != nullptr
               && (dynamic_cast<const ContainerBlock *>(block) != nullptr
                   || dynamic_cast<const FurnaceBlock *>(block) != nullptr
                   || dynamic_cast<const BellBlock *>(block) != nullptr
                   || dynamic_cast<const BannerBlock *>(block) != nullptr
                   || dynamic_cast<const WallBannerBlock *>(block) != nullptr
                   || dynamic_cast<const FlowerPotBlock *>(block) != nullptr
                   || dynamic_cast<const CauldronBlock *>(block) != nullptr
                   || dynamic_cast<const SignBlock *>(block) != nullptr
                   || dynamic_cast<const WallSignBlock *>(block) != nullptr);
    }

    // banners take their dye from the Java block name: Base counts from black
    int32_t bannerBase(const std::string &templateBlock) {
        uint8_t color = DyeColor::fromIdentifier(templateBlock, "_wall_banner");
        if (color == DyeColor::INVALID)
            color = DyeColor::fromIdentifier(templateBlock, "_banner");
        return color == DyeColor::INVALID ? 0 : DyeColor::invert(color);
    }

    // minecraft:potted_<plant>, the plant under its Bedrock name with its default states
    bool pottedPlant(const std::string &templateBlock, BlockState &out) {
        static const std::string PREFIX = "minecraft:potted_";
        if (templateBlock.rfind(PREFIX, 0) != 0)
            return false;

        std::string plant = "minecraft:" + templateBlock.substr(PREFIX.size());
        if (plant == "minecraft:dead_bush")
            plant = "minecraft:deadbush";
        const Tag *defaults = BlockPaletteRegistry::getInstance().getDefaultStates(plant);
        out = BlockState(plant, defaults != nullptr ? *defaults : Tag::ofCompound());
        return true;
    }

    void createBlockActor(BlockActorStore &actors, const VillageStructure::BlockEntity &entity) {
        const Vector3i position(std::get<0>(entity.mPosition), std::get<1>(entity.mPosition),
                                std::get<2>(entity.mPosition));
        const BlockState &state = entity.mBlock;
        const Block *block = VanillaBlocks::fromIdentifier(state.mName);
        if (block == nullptr)
            return;

        std::unique_ptr<BlockActor> actor;
        if (const ContainerBlockDefinition *container = ContainerBlock::findDefinition(state.mName)) {
            actor = ContainerBlock::createBlockActor(container->mKind);
        } else if (dynamic_cast<const FurnaceBlock *>(block) != nullptr) {
            auto furnace = std::make_unique<FurnaceBlockActor>();
            furnace->mKind = FurnaceBlock::kind(state);
            actor = std::move(furnace);
        } else if (dynamic_cast<const BellBlock *>(block) != nullptr) {
            actor = std::make_unique<BellBlockActor>();
        } else if (dynamic_cast<const FlowerPotBlock *>(block) != nullptr) {
            auto pot = std::make_unique<FlowerPotBlockActor>();
            BlockState plant;
            if (pottedPlant(entity.mTemplateBlock, plant))
                pot->setPlant(plant);
            actor = std::move(pot);
        } else if (dynamic_cast<const CauldronBlock *>(block) != nullptr) {
            actor = std::make_unique<CauldronBlockActor>();
        } else if (dynamic_cast<const SignBlock *>(block) != nullptr
                   || dynamic_cast<const WallSignBlock *>(block) != nullptr) {
            // standing and hanging signs share a class; only hanging ones have the state
            actor = std::make_unique<SignBlockActor>(state.mStates.contains("hanging"));
        } else if (dynamic_cast<const BannerBlock *>(block) != nullptr
                   || dynamic_cast<const WallBannerBlock *>(block) != nullptr) {
            auto banner = std::make_unique<BannerBlockActor>();
            banner->setBase(bannerBase(entity.mTemplateBlock));
            actor = std::move(banner);
        }
        if (actor == nullptr)
            return;

        actor->setPosition(position);
        actors.insert(std::move(actor));
    }

    bool insideChunk(int32_t x, int32_t z, int32_t chunkX, int32_t chunkZ) {
        return (x >> 4) == chunkX && (z >> 4) == chunkZ;
    }

    bool touchesChunk(const JigsawBox &box, int32_t chunkX, int32_t chunkZ) {
        return box.mMaxX >= chunkX * 16 && box.mMinX <= chunkX * 16 + 15 && box.mMaxZ >= chunkZ * 16
               && box.mMinZ <= chunkZ * 16 + 15;
    }

    Tag generatedMob(const VillageStructure::Mob &generated) {
        const VillageStructure::Position &position = generated.mPosition;
        Tag location = Tag::ofList(Tag::Type::Float);
        location.addToList(Tag::ofFloat((float) std::get<0>(position) + 0.5f));
        location.addToList(Tag::ofFloat((float) std::get<1>(position)));
        location.addToList(Tag::ofFloat((float) std::get<2>(position) + 0.5f));

        Tag mob = Tag::ofCompound();
        mob.putString("identifier", generated.mIdentifier);
        mob.put("Pos", location);
        if (!generated.mEvent.empty())
            mob.putString(MobActor::TAG_SPAWN_EVENT, generated.mEvent);
        return mob;
    }

    std::string lootTablePath(const std::string &name) {
        const size_t colon = name.find(':');
        return "loot_tables/" + (colon == std::string::npos ? name : name.substr(colon + 1)) + ".json";
    }

    // sub_145C22000, clipped to the chunk: jigsaw final states first (sub_145C24D90), then the file order
    // (sub_14775E430); rules draw from MT(post process seed), reseeded per jigsaw block; chest seeds from the
    // post process random itself, after the feature pieces before it
    class PiecePlacer {
    public:
        PiecePlacer(const ChunkGenerator &generator, const JigsawPiece &piece, int32_t chunkX, int32_t chunkZ,
                    uint32_t seed, std::mt19937 &chunkRandom, StructureTerrain &terrain,
                    VillageStructure::ChunkShare &share, const VillageStructure::Neighbours *neighbours)
            : mGenerator(generator), mPiece(piece), mChunkX(chunkX), mChunkZ(chunkZ), mSeed(seed),
              mChunkRandom(chunkRandom), mTerrain(terrain), mShare(share), mNeighbours(neighbours),
              mRules(rulesOf(*piece.mElement)) {}

        void place(const StructureTemplate &shape) {
            for (const StructureTemplate::BlockEntry &entry : shape.getBlocks()) {
                if (shape.getPalette()[(size_t) entry.mPaletteIndex].mName == "minecraft:jigsaw")
                    _placeJigsaw(entry);
            }

            const std::shared_ptr<const TranslatedPalette> translated = translatedPalette(shape, mPiece.mQuarterTurns);
            std::mt19937 random(mSeed);
            for (const StructureTemplate::BlockEntry &entry : shape.getBlocks()) {
                const size_t index = (size_t) entry.mPaletteIndex;
                _placeBlock(shape.getPalette()[index], (*translated)[index], entry, random);
            }
            if (!mPiece.mElement->mTerrainMatching)
                _foundation();
        }

    private:
        // sub_147761B80: each column of the box with a block on the bottom layer draws nextInt(3), wherever it is;
        // the columns of this chunk then fill down through air and water with the foundation block
        // sub_14699EA50: the biome's top material, dirt instead of grass or without one
        static BlockState foundationBlock(int32_t biome) {
            static const BlockState DIRT = VanillaBlocks::DIRT().toBlockState();
            const SurfaceBuilderComponent *surface = SurfaceBuilderComponent::getForBiome(biome);
            const BlockState *top = surface != nullptr ? surface->mSurfaceMaterial.mTopBlock : nullptr;
            return top == nullptr || top->mName == "minecraft:grass_block" ? DIRT : *top;
        }

        void _foundation() {
            const JigsawBox &box = mPiece.mBox;
            for (int32_t x = box.mMinX; x <= box.mMaxX; ++x) {
                for (int32_t z = box.mMinZ; z <= box.mMaxZ; ++z) {
                    const bool inside = insideChunk(x, z, mChunkX, mChunkZ);
                    const BlockState *done =
                            inside || mNeighbours == nullptr ? nullptr : (*mNeighbours)(x, box.mMinY, z);
                    const BlockState bottom = done != nullptr ? *done : _world({x, box.mMinY, z});
                    if (DecorationSupport::isAir(bottom))
                        continue;

                    const int32_t depth = FeatureRandom::nextInt(mChunkRandom, 3);
                    if (!inside)
                        continue;

                    for (int32_t y = box.mMinY - 1; y > box.mMinY - depth - 3; --y) {
                        const BlockState below = _world({x, y, z});
                        if (!DecorationSupport::isAir(below) && below.mName != "minecraft:water")
                            break;
                        mShare.mBlocks[{x, y, z}] = foundationBlock(mGenerator.pickBiome(x, box.mMinY, z));
                    }
                }
            }
        }

        bool _position(const StructureTemplate::BlockEntry &entry, VillageStructure::Position &out) {
            int32_t x = entry.x;
            int32_t z = entry.z;
            StructureTemplate::rotate(mPiece.mQuarterTurns, x, z);
            x += mPiece.x;
            z += mPiece.z;
            if (!insideChunk(x, z, mChunkX, mChunkZ))
                return false;

            // top of the column, water included
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
            mShare.mBlocks[position] = StructureProcessors::apply(mRules.mRules, block, _world(position), random);
        }

        void _placeBlock(const StructureTemplate::PaletteEntry &palette, const TranslatedEntry &translated,
                         const StructureTemplate::BlockEntry &entry, std::mt19937 &random) {
            if (palette.mName == "minecraft:jigsaw")
                return;

            VillageStructure::Position position;
            if (!_position(entry, position))
                return;

            if (palette.mName == "minecraft:structure_block") {
                _spawn(entry.mData.getString("metadata", std::string()), position);
                return;
            }

            if (!translated.mPlaced)
                return;

            const BlockState block = StructureProcessors::apply(mRules.mRules, translated.mBlock, _world(position),
                                                                random);
            mShare.mBlocks[position] = block;
            if (block.mName == "minecraft:bed") {
                const uint8_t color = DyeColor::fromIdentifier(palette.mName, "_bed");
                if (color != DyeColor::INVALID)
                    mShare.mBeds.push_back({position, color});
            }

            if (hasBlockActor(block.mName))
                mShare.mBlockEntities.push_back({position, block, palette.mName});

            if (entry.mData.getString("id", std::string()) != "minecraft:chest" || block.mName != "minecraft:chest")
                return;

            VillageStructure::Chest chest;
            chest.mPosition = position;
            chest.mLootTable = lootTablePath(entry.mData.getString("LootTable", std::string()));
            chest.mLootSeed = entry.mData.contains("LootTableSeed") ? entry.mData.getInt("LootTableSeed", 0)
                                                                    : FeatureRandom::nextInt(mChunkRandom);
            if (!entry.mData.contains("LootTable"))
                chest.mLootTable.clear();
            mShare.mChests.push_back(chest);
        }

        void _spawn(const std::string &metadata, const VillageStructure::Position &position) {
            std::string mob;
            std::string event;
            mobOf(metadata, mob, event);
            if (mob.empty())
                return;

            // abandoned_golems, abandoned_mobs (sub_14CD6B070)
            if (mob == "minecraft:iron_golem" && hasProcessor(mRules.mProcessors, ABANDONED_GOLEMS))
                return;
            // the metadata becomes minecraft:zombie_villager_v2:from_village
            if (mob == "minecraft:villager_v2" && hasProcessor(mRules.mProcessors, ABANDONED_MOBS)) {
                mob = "minecraft:zombie_villager_v2";
                event = "from_village";
            }

            mShare.mMobs.push_back({position, mob, event});
        }

        const ChunkGenerator &mGenerator;
        const JigsawPiece &mPiece;
        int32_t mChunkX;
        int32_t mChunkZ;
        uint32_t mSeed;
        std::mt19937 &mChunkRandom;
        StructureTerrain &mTerrain;
        VillageStructure::ChunkShare &mShare;
        const VillageStructure::Neighbours *mNeighbours;
        const ElementRules &mRules;
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
    // the plan reads only the seed and the dimension: every worker's generator shares it
    using Key = std::tuple<int64_t, int32_t, int32_t, int32_t>;
    static std::mutex mutex;
    static std::map<Key, std::shared_ptr<const std::vector<JigsawPiece>>> plans;
    static std::deque<Key> order;

    const Key key = {generator.getSeed(), (int32_t) generator.getDimensionType(), startX, startZ};
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = plans.find(key);
        if (found != plans.end())
            return found->second;
    }

    const StructureFeature *village = StructureFeature::get("village");
    if (village == nullptr || !village->isStartChunk(generator, startX, startZ))
        return nullptr;

    // planned unlocked: other workers keep planning
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

std::vector<std::shared_ptr<const std::vector<JigsawPiece>>> VillageStructure::plansNear(
        const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) {
    std::vector<std::shared_ptr<const std::vector<JigsawPiece>>> found;
    const StructureFeature *village = StructureFeature::get("village");
    if (village == nullptr)
        return found;

    for (const StructureChunkPos &start : village->getCandidatesIn(generator.getSeed(), chunkX - REACH, chunkZ - REACH,
                                                                    chunkX + REACH, chunkZ + REACH)) {
        std::shared_ptr<const std::vector<JigsawPiece>> pieces = cachedPlan(generator, start.x, start.z);
        if (pieces != nullptr)
            found.push_back(std::move(pieces));
    }
    return found;
}

VillageStructure::ChunkShare VillageStructure::placeInChunk(const ChunkGenerator &generator,
                                                            const std::vector<JigsawPiece> &pieces, int32_t chunkX,
                                                            int32_t chunkZ, StructureTerrain &terrain,
                                                            BlockManager *features, const Neighbours *neighbours) {
    ChunkShare share;
    const uint32_t seed = StructureFeature::getPostProcessSeed(generator.getSeed(), chunkX, chunkZ);
    std::mt19937 chunkRandom(seed);
    for (const JigsawPiece &piece : pieces) {
        // chunk post process random (sub_1469B1230)
        if (piece.mElement->mFeature != nullptr) {
            if (features != nullptr && insideChunk(piece.x, piece.z, chunkX, chunkZ))
                OverworldRulePlacer::placeFeature(*features, piece.mElement->mFeature, piece.x, piece.y, piece.z,
                                                  chunkRandom);
            continue;
        }

        if (piece.mElement->mTemplate == nullptr || !touchesChunk(piece.mBox, chunkX, chunkZ))
            continue;

        const std::shared_ptr<const StructureTemplate> shape = StructureTemplate::load(piece.mElement->mTemplate);
        if (shape == nullptr)
            continue;

        PiecePlacer(generator, piece, chunkX, chunkZ, seed, chunkRandom, terrain, share, neighbours).place(*shape);
    }
    return share;
}

void VillageStructure::addBeard(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ,
                                TerrainBeard &beard) {
    for (const std::shared_ptr<const std::vector<JigsawPiece>> &pieces : plansNear(generator, chunkX, chunkZ)) {
        // rigid pieces only: streets follow the ground
        for (const JigsawPiece &piece : *pieces) {
            if (piece.mElement->mTerrainMatching)
                continue;

            const JigsawBox &box = piece.mBox;
            beard.addPiece(chunkX, chunkZ,
                           {box.mMinX, box.mMinY, box.mMinZ, box.mMaxX, box.mMaxY, box.mMaxZ,
                            piece.mGroundLevelDelta});
        }
    }
}

void VillageStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk,
                             std::vector<GeneratedBlockChange> *overflow) {
    // shared by the villages: they overlap and read the same columns
    StructureTerrain terrain(generator, level.getTerrainCache());
    for (const std::shared_ptr<const std::vector<JigsawPiece>> &pieces :
         plansNear(generator, chunk.getX(), chunk.getZ())) {
        bool reaches = false;
        for (const JigsawPiece &piece : *pieces)
            reaches = reaches || touchesChunk(piece.mBox, chunk.getX(), chunk.getZ());
        if (!reaches)
            continue;

        BlockManager features(level);
        features.setOverflowSink(chunk.getX(), chunk.getZ(), overflow);
        // the parts in chunks post-processed earlier read their blocks, the others the terrain
        const Neighbours neighbours = [&level, &chunk](int32_t x, int32_t y, int32_t z) -> const BlockState * {
            if ((x >> 4) == chunk.getX() && (z >> 4) == chunk.getZ())
                return nullptr;
            LevelChunk *other = level.peekChunkPtr(x >> 4, z >> 4);
            return other != nullptr && other->isPopulated() ? &other->getBlock(x & 15, y, z & 15) : nullptr;
        };
        const ChunkShare own = placeInChunk(generator, *pieces, chunk.getX(), chunk.getZ(), terrain, &features,
                                            &neighbours);

        // shapes connect across chunk borders
        std::map<Position, BlockState> around = own.mBlocks;
        std::vector<std::pair<LevelChunk *, ChunkShare>> staged;
        for (int32_t dx = -1; dx <= 1; ++dx) {
            for (int32_t dz = -1; dz <= 1; ++dz) {
                if (dx == 0 && dz == 0)
                    continue;
                // a decorated neighbour placed its own share: read as it is, never rewritten
                LevelChunk *neighbourChunk = level.peekChunkPtr(chunk.getX() + dx, chunk.getZ() + dz);
                if (neighbourChunk != nullptr && neighbourChunk->isPopulated())
                    continue;

                ChunkShare next = placeInChunk(generator, *pieces, chunk.getX() + dx, chunk.getZ() + dz, terrain);
                around.insert(next.mBlocks.begin(), next.mBlocks.end());
                if (neighbourChunk != nullptr)
                    staged.emplace_back(neighbourChunk, std::move(next));
            }
        }

        const auto neighbour = [&](int32_t x, int32_t y, int32_t z) -> BlockState {
            const auto found = around.find({x, y, z});
            if (found != around.end())
                return found->second;
            const BlockState *decorated = neighbours(x, y, z);
            return decorated != nullptr ? *decorated : terrain.getBlock(x, y, z);
        };
        const auto write = [&](LevelChunk &target, const ChunkShare &share) {
            for (const auto &entry : share.mBlocks) {
                const int32_t x = std::get<0>(entry.first);
                const int32_t y = std::get<1>(entry.first);
                const int32_t z = std::get<2>(entry.first);
                target.setBlock(x & 15, y, z & 15, StructureConnections::shaped(entry.second, x, y, z, neighbour));
            }
        };

        write(chunk, own);
        // neighbour features then meet the village whatever the post process order
        for (const auto &entry : staged)
            write(*entry.first, entry.second);

        features.applySubChunkUpdate();

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

        for (const BlockEntity &entity : own.mBlockEntities)
            createBlockActor(level.getBlockActors(), entity);

        for (const Mob &mob : own.mMobs)
            level.addGeneratedEntity(generatedMob(mob));
    }
}
