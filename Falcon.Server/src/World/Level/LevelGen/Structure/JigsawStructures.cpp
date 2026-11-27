#include "World/Level/LevelGen/Structure/JigsawStructures.h"

#include "World/Level/Block/Actor/TrialSpawnerBlockActor.h"


#include "Core/Json/Json.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/ScriptStructureRegistry.h"
#include "World/Level/Structure/McStructure.h"
#include "World/Level/Structure/StructureBlockRotation.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/BrushableBlockActor.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/ContainerBlock.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/OverworldGeneratorConstants.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Overworld/Feature/OverworldRulePlacer.h"
#include "World/Level/LevelGen/Random/LegacyRandom.h"
#include "World/Level/LevelGen/Random/XoroshiroRandom.h"
#include "World/Level/LevelGen/Structure/StructureBlockTable.h"
#include "World/Level/LevelGen/Structure/StructureConnections.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"
#include "World/Level/LevelGen/Structure/StructureTerrain.h"
#include "World/Level/Block/BlockNames.h"
#include "WorldgenJson.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <tuple>

namespace {
    const char *const NAMESPACE = "minecraft:";
    const char *const JIGSAW = "minecraft:jigsaw";
    const char *const STRUCTURE_BLOCK = "minecraft:structure_block";
    const char *const STRUCTURE_VOID = "minecraft:structure_void";
    // a camp spans a few dozen chunks: these cover the chunks generated at once
    const size_t CACHED_PLANS = 256;

    using Position = std::tuple<int32_t, int32_t, int32_t>;

    std::string withNamespace(const std::string &name) {
        return name.find(':') == std::string::npos ? NAMESPACE + name : name;
    }

    std::string withoutNamespace(const std::string &name) {
        const size_t colon = name.find(':');
        return colon == std::string::npos ? name : name.substr(colon + 1);
    }

    const json::Value *member(const json::Value *value, const char *key) {
        return value == nullptr || !value->isObject() ? nullptr : value->get(key);
    }

    std::string text(const json::Value *value, const std::string &fallback = std::string()) {
        return value != nullptr && value->isString() ? value->mString : fallback;
    }

    int32_t number(const json::Value *value, int32_t fallback) {
        if (value == nullptr || value->mType != json::Value::Type::Number)
            return fallback;
        const double n = value->mNumber;
        if (!std::isfinite(n) || n < INT32_MIN || n > INT32_MAX || n != std::trunc(n))
            return fallback;
        return (int32_t) n;
    }

    std::optional<int32_t> overworldAnchor(const json::Value *anchor) {
        const auto read = [anchor](const char *name) { return number(member(anchor, name), INT32_MIN); };
        constexpr int32_t LIMIT = INT32_MAX / 4;
        const int32_t absolute = read("absolute");
        if (absolute >= -LIMIT && absolute <= LIMIT)
            return absolute;
        const int32_t aboveBottom = read("above_bottom");
        if (aboveBottom >= 0 && aboveBottom <= LIMIT)
            return LevelChunk::MIN_Y + aboveBottom;
        const int32_t belowTop = read("below_top");
        if (belowTop >= 0 && belowTop <= LIMIT)
            return LevelChunk::MAX_Y - belowTop;
        const int32_t fromSea = read("from_sea");
        if (fromSea >= -LIMIT && fromSea <= LIMIT)
            return OverworldGeneratorConstants::SEA_LEVEL + fromSea;
        return std::nullopt;
    }

    // a constant int provider, or the low end of a uniform one
    int32_t intProvider(const json::Value *value, int32_t fallback) {
        if (value == nullptr)
            return fallback;
        if (value->mType == json::Value::Type::Number)
            return number(value, fallback);
        const json::Value *inner = member(value, "value");
        if (inner != nullptr && inner->mType == json::Value::Type::Number)
            return number(inner, fallback);
        return number(member(value, "min_inclusive"), fallback);
    }

    // the 48 bit generator seeded from the world seed and the chunk, as large features are
    LegacyRandom largeFeatureRandom(int64_t seed, int32_t chunkX, int32_t chunkZ) {
        LegacyRandom base(seed);
        const int64_t a = base.nextLong();
        const int64_t b = base.nextLong();
        return LegacyRandom((int64_t) ((uint64_t) a * (uint64_t) (int64_t) chunkX)
                            ^ (int64_t) ((uint64_t) b * (uint64_t) (int64_t) chunkZ) ^ seed);
    }

    // the game's block position hash, the seed of a piece's capped replacements
    int64_t positionSeed(int32_t x, int32_t y, int32_t z) {
        const int64_t mixed = (116129781LL * (int64_t) z) ^ (int64_t) y ^ (int64_t) (int32_t) ((uint32_t) x * 3129871U);
        const uint64_t spread = (uint64_t) mixed * (42317861ULL * (uint64_t) mixed + 11ULL);
        return (int64_t) (int32_t) (uint32_t) (spread >> 16);
    }

    // the same hash kept whole, the seed of a positional random
    int64_t blockSeed(int32_t x, int32_t y, int32_t z) {
        const int64_t mixed = (116129781LL * (int64_t) z) ^ (int64_t) y ^ (int64_t) (int32_t) ((uint32_t) x * 3129871U);
        return (int64_t) (((uint64_t) mixed * (42317861ULL * (uint64_t) mixed + 11ULL)) >> 16);
    }

    bool insideChunk(int32_t x, int32_t z, int32_t chunkX, int32_t chunkZ) {
        return (x >> 4) == chunkX && (z >> 4) == chunkZ;
    }

    bool touchesChunk(const JigsawPlacer::Box &box, int32_t chunkX, int32_t chunkZ) {
        return box.mMaxX >= chunkX * 16 && box.mMinX <= chunkX * 16 + 15 && box.mMaxZ >= chunkZ * 16
               && box.mMinZ <= chunkZ * 16 + 15;
    }

    BlockState defaultState(const std::string &name) {
        const Tag *defaults = BlockPaletteRegistry::getInstance().getDefaultStates(name);
        return BlockState(name, defaults != nullptr ? *defaults : Tag::ofCompound());
    }

    std::string templatePath(const std::string &location) {
        return withoutNamespace(location) + ".nbt";
    }

    std::shared_ptr<const JigsawPlacer::Template> templateOf(const JigsawPlacer::Element &element) {
        if (!element.mFeature.empty())
            return JigsawPlacer::featureTemplate();

        static std::mutex mutex;
        static std::map<std::string, std::shared_ptr<const JigsawPlacer::Template>> cache;
        const std::lock_guard<std::mutex> lock(mutex);
        const auto found = cache.find(element.mLocation);
        if (found != cache.end())
            return found->second;

        const std::shared_ptr<const StructureTemplate> source = StructureTemplate::load(templatePath(element.mLocation));
        std::shared_ptr<const JigsawPlacer::Template> shape = source == nullptr ? nullptr
                                                                                : JigsawPlacer::templateOf(*source);
        cache.emplace(element.mLocation, shape);
        return shape;
    }

    void readRule(const json::Value &rule, JigsawStructures::ProcessorList &out) {
        const json::Value *input = member(&rule, "input_predicate");
        const std::string type = text(member(input, "predicate_type"), "minecraft:always_true");

        StructureProcessors::Rule converted{};
        if (type == "minecraft:always_true") {
            converted.mInput = StructureProcessors::Test::AlwaysTrue;
        } else if (type == "minecraft:block_match" || type == "minecraft:random_block_match") {
            out.mStrings.push_back(withNamespace(text(member(input, "block"))));
            converted.mInputBlock = out.mStrings.back().c_str();
            converted.mInput = type == "minecraft:block_match" ? StructureProcessors::Test::BlockMatch
                                                               : StructureProcessors::Test::RandomBlockMatch;
            const json::Value *probability = member(input, "probability");
            converted.mProbability = probability != nullptr ? (float) probability->mNumber : 1.0f;
        } else {
            return;
        }

        const json::Value *location = member(&rule, "location_predicate");
        if (text(member(location, "predicate_type")) == "minecraft:block_match") {
            out.mStrings.push_back(withNamespace(text(member(location, "block"))));
            converted.mLocationBlock = out.mStrings.back().c_str();
        }

        out.mStrings.push_back(withNamespace(text(member(member(&rule, "output_state"), "name"))));
        converted.mOutput = out.mStrings.back().c_str();
        out.mRules.push_back(converted);
    }

    // the game turns a capped list of block_match rules into one replacement per block
    void readCapped(const json::Value &processor, JigsawStructures::ProcessorList &out) {
        JigsawStructures::Capped capped;
        capped.mLimit = intProvider(member(&processor, "limit"), 0);
        const json::Value *delegate = member(&processor, "delegate");
        const json::Value *rules = member(delegate, "rules");
        if (rules == nullptr || !rules->isArray())
            return;

        for (const std::unique_ptr<json::Value> &rule: rules->mArray) {
            const json::Value *input = member(rule.get(), "input_predicate");
            if (text(member(input, "predicate_type")) != "minecraft:block_match")
                continue;
            capped.mReplacements[withNamespace(text(member(input, "block")))] =
                    withNamespace(text(member(member(rule.get(), "output_state"), "name")));
            const std::string loot = text(member(member(rule.get(), "block_entity_modifier"), "loot_table"));
            if (!loot.empty())
                capped.mLootTable = loot;
        }
        out.mCapped.push_back(std::move(capped));
    }

    TerrainBeard::Adaptation adaptationOf(const std::string &name, bool &adapts) {
        adapts = true;
        if (name == "bury")
            return TerrainBeard::Adaptation::Bury;
        if (name == "encapsulate")
            return TerrainBeard::Adaptation::Encapsulate;
        if (name == "beard_thin")
            return TerrainBeard::Adaptation::BeardThin;
        adapts = false;
        return TerrainBeard::Adaptation::BeardThin;
    }

    std::string lootPath(const std::string &name) {
        if (name.rfind("loot_tables/", 0) == 0)
            return name;
        return "loot_tables/" + withoutNamespace(name) + ".json";
    }

    std::unique_ptr<BlockActor> containerOf(const BlockState &block) {
        if (block.mName == "minecraft:chest")
            return BlockActorStore::create(ChestBlockActor::BLOCK_ACTOR_ID);
        const ContainerBlockDefinition *container = ContainerBlock::findDefinition(block.mName);
        return container == nullptr ? nullptr : ContainerBlock::createBlockActor(container->mKind);
    }

    Tag generatedActor(const std::string &block, const Tag &data, int32_t x, int32_t y, int32_t z) {
        Tag actor = Tag::ofCompound();
        if (block == "minecraft:trial_spawner") {
            actor = TrialSpawnerBlockActor::createTrialData(data);
            actor.putString("id", TrialSpawnerBlockActor::BLOCK_ACTOR_ID);
        } else if (block == "minecraft:vault") {
            // the templates use a different format for vault keys and loot paths
            const Tag *config = data.get("config");
            const Tag *key = config != nullptr ? config->get("key_item") : nullptr;
            Tag item = Tag::ofCompound();
            item.putByte("Count", (int8_t) (key != nullptr ? key->getInt("count", 1) : 1));
            item.putShort("Damage", 0);
            item.putString("Name", key != nullptr ? key->getString("id", std::string()) : std::string());
            item.putByte("WasPickedUp", 0);
            Tag settings = Tag::ofCompound();
            settings.putFloat("activation_range", 4.0f);
            settings.putFloat("deactivation_range", 4.5f);
            settings.put("key_item", item);
            const std::string table = config != nullptr ? config->getString("loot_table", std::string()) : std::string();
            settings.putString("loot_table", lootPath(table));
            const std::string display =
                    config != nullptr ? config->getString("override_loot_table_to_display") : std::string();
            settings.putString("override_loot_table_to_display", display.empty() ? display : lootPath(display));
            Tag state = Tag::ofCompound();
            state.put("items_to_eject", Tag::ofList(Tag::Type::Compound, {}));
            state.put("rewarded_players", Tag::ofList(Tag::Type::Long));
            state.putLong("state_updating_resumes_at", 0);
            actor.putString("id", "Vault");
            actor.put("config", settings);
            actor.put("data", state);
        } else {
            return Tag();
        }
        actor.putInt("x", x);
        actor.putInt("y", y);
        actor.putInt("z", z);
        return actor;
    }

    struct ChunkShare {
        std::map<Position, BlockState> mBlocks;
        struct Loot {
            Position mPosition;
            std::string mTable;
            int32_t mSeed;
            bool mBrushable;
            std::string mBlock;
        };
        std::vector<Loot> mLoot;
        // generated block actor data, with the block it belongs to
        std::vector<std::pair<Tag, std::string>> mGenerated;
        // the four faces of a decorated pot
        std::map<Position, Tag> mSherds;
    };

    // one piece clipped to one chunk: jigsaws become their final state, rules draw from MT(post process seed)
    // once per piece, capped replacements run on what the piece left
    class PiecePlacer {
    public:
        PiecePlacer(const JigsawStructures &registry, const JigsawPlacer::Piece &piece, int32_t chunkX,
                    int32_t chunkZ, uint32_t seed, std::mt19937 &chunkRandom, StructureTerrain &terrain,
                    ChunkShare &share)
                : mPiece(piece), mChunkX(chunkX), mChunkZ(chunkZ), mRandom(seed),
                  mChunkRandom(chunkRandom), mTerrain(terrain), mShare(share),
                  mList(registry.processors(piece.mElement->mProcessors)) {
            if (mList != nullptr) {
                for (const StructureProcessors::Rule &rule: mList->mRules)
                    mRules.push_back(&rule);
            }
        }

        void place(const StructureTemplate &shape) {
            for (const StructureTemplate::BlockEntry &entry: shape.getBlocks()) {
                if (shape.getPalette()[(size_t) entry.mPaletteIndex].mName == JIGSAW)
                    _placeJigsaw(entry);
            }

            std::vector<std::vector<Position>> candidates(mList == nullptr ? 0 : mList->mCapped.size());
            for (const StructureTemplate::BlockEntry &entry: shape.getBlocks()) {
                const StructureTemplate::PaletteEntry &palette = shape.getPalette()[(size_t) entry.mPaletteIndex];
                if (palette.mName == JIGSAW || palette.mName == STRUCTURE_BLOCK || palette.mName == STRUCTURE_VOID)
                    continue;

                Position position;
                BlockState block;
                if (!_position(entry, position) || !StructureBlockTable::translate(palette, mPiece.mQuarterTurns, block))
                    continue;

                _placeBlock(position, block, candidates);
                _loot(entry, position, block);
            }

            _replaceCapped(candidates);
        }

        void place(const McStructure &shape, LevelChunk &chunk) {
            const Vector3i size = shape.getSize();
            std::vector<std::vector<Position>> candidates(mList == nullptr ? 0 : mList->mCapped.size());
            for (int32_t x = 0; x < size.x; ++x) {
                for (int32_t z = 0; z < size.z; ++z) {
                    for (int32_t y = 0; y < size.y; ++y) {
                        const Vector3i local(x, y, z);
                        const BlockState *state = shape.getBlock(local);
                        if (state == nullptr || state->mName == STRUCTURE_BLOCK || state->mName == STRUCTURE_VOID)
                            continue;
                        Position position;
                        if (!_position(x, y, z, position))
                            continue;
                        const auto &[worldX, worldY, worldZ] = position;
                        const Tag *data = shape.getBlockEntity(local);
                        const bool jigsaw = state->mName == JIGSAW;
                        BlockState block;
                        if (jigsaw) {
                            if (data == nullptr || !StructureBlockTable::translateFinalState(
                                    data->getString("final_state", "minecraft:air"), mPiece.mQuarterTurns, block))
                                continue;
                        } else {
                            block = StructureBlockRotation::apply(*state, mPiece.mQuarterTurns, 0);
                        }
                        const std::string originalName = data != nullptr && !jigsaw ? block.mName : std::string();
                        _placeBlock(position, block, candidates);
                        if (data != nullptr && !jigsaw && originalName == block.mName) {
                            Tag placed = *data;
                            placed.putInt("x", worldX);
                            placed.putInt("y", worldY);
                            placed.putInt("z", worldZ);
                            mShare.mGenerated.emplace_back(std::move(placed), block.mName);
                        }
                        if (const BlockState *overlay = shape.getBlock(local, 1)) {
                            chunk.setBlock(worldX & 15, worldY, worldZ & 15, 1,
                                           StructureBlockRotation::apply(*overlay, mPiece.mQuarterTurns, 0));
                        }
                    }
                }
            }
            _replaceCapped(candidates);
        }

    private:
        void _placeBlock(const Position &position, BlockState &block, std::vector<std::vector<Position>> &candidates) {
            if (!mRules.empty())
                block = StructureProcessors::apply(mRules, block, _world(position), mRandom);
            mShare.mBlocks[position] = block;
            for (size_t index = 0; index < candidates.size(); ++index) {
                if (mList->mCapped[index].mReplacements.count(block.mName) != 0)
                    candidates[index].push_back(position);
            }
        }

        void _replaceCapped(std::vector<std::vector<Position>> &candidates) {
            for (size_t index = 0; index < candidates.size(); ++index)
                _replace(mList->mCapped[index], candidates[index]);
        }

        bool _position(const StructureTemplate::BlockEntry &entry, Position &out) {
            return _position(entry.x, entry.y, entry.z, out);
        }

        bool _position(int32_t x, int32_t localY, int32_t z, Position &out) {
            StructureTemplate::rotate(mPiece.mQuarterTurns, x, z);
            x += mPiece.x;
            z += mPiece.z;
            if (!insideChunk(x, z, mChunkX, mChunkZ))
                return false;

            const int32_t y = mPiece.mElement->mTerrainMatching ? mTerrain.getTopHeight(x, z) - 1 + localY
                                                                : mPiece.y + localY;
            if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
                return false;

            out = {x, y, z};
            return true;
        }

        BlockState _world(const Position &position) {
            const auto placed = mShare.mBlocks.find(position);
            if (placed != mShare.mBlocks.end())
                return placed->second;
            return mTerrain.getBlock(std::get<0>(position), std::get<1>(position), std::get<2>(position));
        }

        void _placeJigsaw(const StructureTemplate::BlockEntry &entry) {
            Position position;
            if (!_position(entry, position))
                return;

            BlockState block;
            if (!StructureBlockTable::translateFinalState(entry.mData.getString("final_state", "minecraft:air"),
                                                          mPiece.mQuarterTurns, block))
                return;

            mShare.mBlocks[position] = StructureProcessors::apply(mRules, block, _world(position), mRandom);
        }

        // a container of the template keeps its loot table; its seed is the chunk's next draw when the template
        // gives none
        void _loot(const StructureTemplate::BlockEntry &entry, const Position &position, const BlockState &block) {
            Tag generated = generatedActor(block.mName, entry.mData, std::get<0>(position), std::get<1>(position),
                                           std::get<2>(position));
            if (generated.isCompound()) {
                mShare.mGenerated.emplace_back(std::move(generated), block.mName);
                return;
            }
            const Tag *sherds = entry.mData.get("sherds");
            if (sherds != nullptr && sherds->isList())
                mShare.mSherds[position] = *sherds;
            if (!entry.mData.contains("LootTable") || containerOf(block) == nullptr)
                return;

            const int32_t seed = entry.mData.contains("LootTableSeed") ? entry.mData.getInt("LootTableSeed", 0)
                                                                       : FeatureRandom::nextInt(mChunkRandom);
            mShare.mLoot.push_back({position, lootPath(entry.mData.getString("LootTable", std::string())), seed, false,
                                    block.mName});
        }

        // candidates shuffled from the second on, each one still a replaceable block taken until the limit
        void _replace(const JigsawStructures::Capped &capped, std::vector<Position> &positions) {
            XoroshiroRandom random(positionSeed(mPiece.x, mPiece.y, mPiece.z));
            for (size_t i = 1; i < positions.size(); ++i) {
                const size_t j = (size_t) random.nextInt((int32_t) i + 1);
                if (j != i)
                    std::swap(positions[i], positions[j]);
            }

            int32_t replaced = 0;
            for (const Position &position: positions) {
                if (replaced >= capped.mLimit)
                    return;
                const auto found = capped.mReplacements.find(_world(position).mName);
                if (found == capped.mReplacements.end())
                    continue;

                mShare.mBlocks[position] = defaultState(found->second);
                if (!capped.mLootTable.empty())
                    mShare.mLoot.push_back({position, capped.mLootTable, random.nextInt(), true, found->second});
                ++replaced;
            }
        }

        const JigsawPlacer::Piece &mPiece;
        int32_t mChunkX;
        int32_t mChunkZ;
        // one generator for the whole piece, its jigsaws included
        std::mt19937 mRandom;
        std::mt19937 &mChunkRandom;
        StructureTerrain &mTerrain;
        ChunkShare &mShare;
        const JigsawStructures::ProcessorList *mList;
        std::vector<const StructureProcessors::Rule *> mRules;
    };

    class SetFeature final : public StructureFeature {
    public:
        SetFeature(const JigsawStructures &registry, const JigsawStructures::Set &set, bool registerFeature)
                : StructureFeature(withoutNamespace(set.mIdentifier), set.mSpacing, set.mSeparation, set.mSalt,
                                   set.mSpread, Generator::Congruential, registerFeature),
                  mRegistry(registry), mSet(set) {}

    protected:
        bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override {
            return mRegistry.plan(generator, mSet, chunkX, chunkZ) != nullptr;
        }

    private:
        const JigsawStructures &mRegistry;
        const JigsawStructures::Set &mSet;
    };
}

JigsawStructures &JigsawStructures::get() {
    static JigsawStructures *structures = [] {
        auto *loaded = new JigsawStructures();
        const std::unique_ptr<json::Value> document = json::parse(FalconWorldgenData::kWorldgenJson);
        if (document != nullptr)
            loaded->load(*document);
        loaded->_loadTrialChambers();
        return loaded;
    }();
    return *structures;
}

JigsawStructures &JigsawStructures::packStructures() {
    static JigsawStructures registry(true);
    return registry;
}

void JigsawStructures::loadPacks(const std::vector<BehaviorPack> &packs) {
    namespace fs = std::filesystem;
    auto document = json::Value::ofObject();
    ScriptStructureRegistry templates;
    for (const BehaviorPack &pack: packs) {
        templates.addPack(pack.mPath);
        for (const std::string folder: {"processors", "template_pools", "jigsaw_structures", "structure_sets"}) {
            const fs::path root = fs::path(pack.mPath) / "worldgen" / folder;
            std::error_code error;
            std::vector<fs::path> paths;
            for (fs::recursive_directory_iterator it(root, error), end; !error && it != end; it.increment(error)) {
                if (it->is_regular_file(error) && it->path().extension() == ".json")
                    paths.push_back(it->path());
            }
            std::sort(paths.begin(), paths.end());
            for (const fs::path &path: paths) {
                std::ifstream file(path, std::ios::binary);
                const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                auto data = json::parse(bytes);
                if (!data || !data->isObject())
                    continue;
                const std::string type = folder == "jigsaw_structures" ? "structures" : folder;
                document->set(type + "/" + path.generic_string(), std::move(data));
            }
        }
    }

    mPlans.clear();
    mPlanOrder.clear();
    mFeatures.clear();
    mSets.clear();
    mStructures.clear();
    mProcessors.clear();
    mPools.clear();
    mElements.clear();
    mPackShapes.clear();
    mPackTemplates.clear();
    load(*document);
    for (const auto &element: mElements) {
        if (element->mLocation.empty() || mPackTemplates.contains(element->mLocation))
            continue;
        auto structure = templates.find(element->mLocation);
        if (structure == nullptr)
            continue;

        auto shape = std::make_shared<JigsawPlacer::Template>();
        const Vector3i size = structure->getSize();
        shape->mSizeX = size.x;
        shape->mSizeY = size.y;
        shape->mSizeZ = size.z;
        for (int32_t x = 0; x < size.x; ++x) {
            for (int32_t y = 0; y < size.y; ++y) {
                for (int32_t z = 0; z < size.z; ++z) {
                    const Vector3i local(x, y, z);
                    const BlockState *state = structure->getBlock(local);
                    const Tag *tag = structure->getBlockEntity(local);
                    if (!state || state->mName != JIGSAW || !tag)
                        continue;
                    const int32_t facing = state->mStates.getInt("facing_direction", -1);
                    if (facing < 0 || facing > 5)
                        continue;
                    JigsawPlacer::Jigsaw marker;
                    marker.x = x;
                    marker.y = y;
                    marker.z = z;
                    marker.mFront = (JigsawPlacer::Facing) facing;
                    marker.mTop = JigsawPlacer::Facing(1);
                    marker.mName = tag->getString("name");
                    marker.mTarget = tag->getString("target", marker.mName);
                    marker.mPool = tag->getString("target_pool");
                    marker.mFinalState = tag->getString("final_state");
                    marker.mAligned = tag->getString("joint") == "aligned";
                    marker.mPlacementPriority = tag->getInt("placement_priority");
                    marker.mSelectionPriority = tag->getInt("selection_priority");
                    shape->mJigsaws.push_back(std::move(marker));
                }
            }
        }
        mPackShapes.emplace(element->mLocation, std::move(shape));
        mPackTemplates.emplace(element->mLocation, std::move(structure));
    }
}

void JigsawStructures::load(const json::Value &document) {
    for (const std::string &key: document.mKeys) {
        const json::Value &entry = *document.mObject.at(key);

        if (key.rfind("processors/", 0) == 0) {
            const json::Value *list = member(&entry, "minecraft:processor_list");
            const std::string name = text(member(member(list, "description"), "identifier"));
            const json::Value *processors = member(list, "processors");
            if (name.empty() || processors == nullptr || !processors->isArray())
                continue;

            ProcessorList &out = mProcessors[name];
            out = ProcessorList();
            for (const std::unique_ptr<json::Value> &processor: processors->mArray) {
                const std::string type = text(member(processor.get(), "processor_type"));
                const json::Value *rules = member(processor.get(), "rules");
                if (type == "minecraft:rule" && rules != nullptr && rules->isArray()) {
                    for (const std::unique_ptr<json::Value> &rule: rules->mArray)
                        readRule(*rule, out);
                } else if (type == "minecraft:capped") {
                    readCapped(*processor, out);
                }
            }
            continue;
        }

        if (key.rfind("template_pools/", 0) == 0) {
            const json::Value *pool = member(&entry, "minecraft:template_pool");
            const std::string name = text(member(member(pool, "description"), "identifier"));
            const json::Value *elements = member(pool, "elements");
            if (name.empty() || elements == nullptr || !elements->isArray())
                continue;

            JigsawPlacer::Pool &out = mPools[name];
            out.mSlots.clear();
            out.mFallback = text(member(pool, "fallback"), "minecraft:empty");
            for (const std::unique_ptr<json::Value> &weighted: elements->mArray) {
                const json::Value *element = member(weighted.get(), "element");
                const std::string type = text(member(element, "element_type"));
                const int32_t weight = number(member(weighted.get(), "weight"), 1);
                if (weight < 1 || weight > 200)
                    continue;
                auto converted = std::make_unique<JigsawPlacer::Element>();
                if (type == "minecraft:single_pool_element") {
                    converted->mLocation = text(member(element, "location"));
                    converted->mProcessors = text(member(element, "processors"));
                    converted->mTerrainMatching = text(member(element, "projection")) == "terrain_matching";
                } else if (type == "minecraft:feature_pool_element") {
                    converted->mFeature = text(member(element, "feature"));
                } else if (type != "minecraft:empty_pool_element") {
                    continue;
                }

                const JigsawPlacer::Element *kept = nullptr;
                if (!converted->mLocation.empty() || !converted->mFeature.empty()) {
                    mElements.push_back(std::move(converted));
                    kept = mElements.back().get();
                }
                // one slot per weight unit
                for (int32_t copy = 0; copy < weight; ++copy)
                    out.mSlots.push_back(kept);
            }
            continue;
        }

        if (key.rfind("structures/", 0) == 0) {
            const json::Value *jigsaw = member(&entry, "minecraft:jigsaw");
            Structure structure;
            structure.mIdentifier = text(member(member(jigsaw, "description"), "identifier"));
            if (structure.mIdentifier.empty())
                continue;

            const json::Value *filters = member(jigsaw, "biome_filters");
            if (filters != nullptr && filters->isArray()) {
                for (const std::unique_ptr<json::Value> &filter: filters->mArray) {
                    if (text(member(filter.get(), "test")) != "has_biome_tag")
                        continue;
                    structure.mBiomeFilter.push_back(
                            {text(member(filter.get(), "value")), text(member(filter.get(), "operator"), "==") != "!="});
                }
            }

            structure.mStartPool = text(member(jigsaw, "start_pool"));
            structure.mStartJigsaw = text(member(jigsaw, "start_jigsaw_name"));
            structure.mMaxDepth = number(member(jigsaw, "max_depth"), 0);
            if (structure.mMaxDepth < 0 || structure.mMaxDepth > 20)
                continue;
            const json::Value *distance = member(jigsaw, "max_distance_from_center");
            if (distance != nullptr && distance->mType == json::Value::Type::Number) {
                structure.mMaxDistanceHorizontal = number(distance, 80);
                structure.mMaxDistanceVertical = number(distance, 80);
            } else {
                structure.mMaxDistanceHorizontal = number(member(distance, "horizontal"), 80);
                structure.mMaxDistanceVertical =
                        number(member(distance, "vertical"), mFromPacks ? INT32_MAX : 80);
            }
            if (structure.mMaxDistanceHorizontal < 1 || structure.mMaxDistanceHorizontal > 128
                || structure.mMaxDistanceVertical < 1)
                continue;
            const auto *height = member(jigsaw, "start_height");
            if (mFromPacks) {
                const std::string type = text(member(height, "type"), "constant");
                if (type == "constant") {
                    const auto start = overworldAnchor(member(height, "value"));
                    if (!start)
                        continue;
                    structure.mStartY = structure.mStartYMax = *start;
                } else if (type == "uniform") {
                    const auto min = overworldAnchor(member(height, "min"));
                    const auto max = overworldAnchor(member(height, "max"));
                    if (!min || !max || *min > *max)
                        continue;
                    structure.mStartY = *min;
                    structure.mStartYMax = *max;
                } else {
                    continue;
                }
            } else {
                structure.mStartY = number(member(member(height, "value"), "absolute"), 0);
            }

            const auto *padding = member(jigsaw, "dimension_padding");
            const int32_t symmetricPadding = std::max(0, number(padding, 0));
            structure.mPaddingTop = std::max(0, number(member(padding, "top"), symmetricPadding));
            structure.mPaddingBottom = std::max(0, number(member(padding, "bottom"), symmetricPadding));
            const std::string projection = text(member(jigsaw, "heightmap_projection"));
            structure.mProjection = projection == "world_surface" ? JigsawPlacer::Projection::WorldSurface
                                    : projection == "ocean_floor" ? JigsawPlacer::Projection::OceanFloor
                                                                  : JigsawPlacer::Projection::None;
            structure.mAdaptation = adaptationOf(text(member(jigsaw, "terrain_adaptation")), structure.mAdapts);
            mStructures[structure.mIdentifier] = std::move(structure);
        }
    }

    for (const std::string &key: document.mKeys) {
        if (key.rfind("structure_sets/", 0) != 0)
            continue;

        const json::Value *set = member(document.mObject.at(key).get(), "minecraft:structure_set");
        const json::Value *placement = member(set, "placement");
        if (text(member(placement, "type")) != "minecraft:random_spread")
            continue;

        auto out = std::make_unique<Set>();
        out->mIdentifier = text(member(member(set, "description"), "identifier"));
        out->mSalt = number(member(placement, "salt"), 0);
        out->mSpacing = number(member(placement, "spacing"), 1);
        out->mSeparation = number(member(placement, "separation"), 0);
        if (out->mIdentifier.empty() || out->mSeparation < 0 || out->mSeparation >= out->mSpacing)
            continue;
        out->mSpread = text(member(placement, "spread_type")) == "triangular" ? StructureFeature::Spread::Triangular
                                                                              : StructureFeature::Spread::Linear;
        const json::Value *frequency = member(placement, "frequency");
        out->mFrequency = frequency != nullptr ? (float) frequency->mNumber : 1.0f;

        const json::Value *structures = member(set, "structures");
        if (structures == nullptr || !structures->isArray())
            continue;
        int32_t totalWeight = 0;
        for (const std::unique_ptr<json::Value> &weighted: structures->mArray) {
            const Structure *structure = this->structure(text(member(weighted.get(), "structure")));
            const int32_t weight = number(member(weighted.get(), "weight"), 1);
            if (structure == nullptr || weight <= 0 || weight > INT32_MAX - totalWeight)
                continue;
            out->mStructures.push_back({structure, weight});
            totalWeight += weight;
        }
        if (out->mStructures.empty())
            continue;

        _addSet(std::move(out));
    }
}

void JigsawStructures::_addSet(std::unique_ptr<Set> set) {
    mSets.push_back(std::move(set));
    mFeatures.push_back(std::make_unique<SetFeature>(*this, *mSets.back(), !mFromPacks));
}

const JigsawPlacer::Pool *JigsawStructures::pool(const std::string &name) const {
    // the game registers minecraft:empty as a pool without elements: the fallback every branch ends on
    static const JigsawPlacer::Pool EMPTY;
    const std::string identifier = withNamespace(name);
    const auto found = mPools.find(identifier);
    if (found != mPools.end())
        return &found->second;
    return identifier == "minecraft:empty" ? &EMPTY : nullptr;
}

const JigsawStructures::ProcessorList *JigsawStructures::processors(const std::string &name) const {
    if (name.empty())
        return nullptr;
    const auto found = mProcessors.find(withNamespace(name));
    return found == mProcessors.end() ? nullptr : &found->second;
}

const JigsawStructures::Structure *JigsawStructures::structure(const std::string &name) const {
    const auto found = mStructures.find(withNamespace(name));
    return found == mStructures.end() ? nullptr : &found->second;
}

bool JigsawStructures::passes(const Structure &structure, int32_t biomeId) {
    if (!structure.mBiomes.empty()
        && std::find(structure.mBiomes.begin(), structure.mBiomes.end(), biomeId) == structure.mBiomes.end())
        return false;
    for (const BiomeTest &test: structure.mBiomeFilter) {
        if (BiomeChunkGenDataRegistry::hasTag(biomeId, test.mTag) != test.mEquals)
            return false;
    }
    return true;
}

std::map<std::string, std::string> JigsawStructures::_resolveAliases(const Structure &structure, int64_t seed,
                                                                    int32_t x, int32_t y, int32_t z) {
    std::map<std::string, std::string> resolved;
    if (structure.mAliases.empty())
        return resolved;

    // the world seed's random forked at the start position, apart from the start's own random
    LegacyRandom base(seed);
    const int64_t forked = base.nextLong();
    LegacyRandom random(blockSeed(x, y, z) ^ forked);
    for (const AliasBinding &binding: structure.mAliases) {
        int32_t total = 0;
        for (const AliasBinding::Choice &choice: binding.mChoices)
            total += choice.mWeight;
        int32_t draw = total != 0 ? random.nextInt(total) : 0;
        for (const AliasBinding::Choice &choice: binding.mChoices) {
            draw -= choice.mWeight;
            if (draw >= 0)
                continue;
            for (const std::pair<std::string, std::string> &target: choice.mTargets)
                resolved[target.first] = target.second;
            break;
        }
    }
    return resolved;
}

std::shared_ptr<const JigsawStructures::Plan> JigsawStructures::_generate(const ChunkGenerator &generator,
                                                                          const Structure &structure, int32_t chunkX,
                                                                          int32_t chunkZ) const {
    LegacyRandom random = largeFeatureRandom(generator.getSeed(), chunkX, chunkZ);
    // the height is the start's first draw
    int32_t startY = structure.mStartY;
    if (structure.mStartYMax > structure.mStartY)
        startY += random.nextInt(structure.mStartYMax - structure.mStartY + 1);

    const std::map<std::string, std::string> aliases =
            _resolveAliases(structure, generator.getSeed(), chunkX * 16, startY, chunkZ * 16);
    const auto nextInt = [&random](int32_t bound) { return random.nextInt(bound); };
    const auto pools = [this, &aliases](const std::string &name) {
        const auto aliased = aliases.find(withNamespace(name));
        return pool(aliased == aliases.end() ? name : aliased->second);
    };

    // the noise column, water included, the ground the game projects the start on
    std::map<std::pair<int32_t, int32_t>, std::array<int32_t, 256>> heights;
    const auto height = [&generator, &heights](int32_t x, int32_t z, JigsawPlacer::Projection) {
        const std::pair<int32_t, int32_t> key(x >> 4, z >> 4);
        auto found = heights.find(key);
        if (found == heights.end())
            found = heights.emplace(key, generator.getPreliminaryHeights(key.first, key.second)).first;
        return found->second[(size_t) (((z & 15) << 4) | (x & 15))];
    };

    JigsawPlacer::Settings settings;
    settings.mStartPool = structure.mStartPool;
    settings.mStartJigsaw = structure.mStartJigsaw;
    settings.mMaxDepth = structure.mMaxDepth;
    settings.mKeepStartAtZeroDepth = true;
    settings.mStartX = chunkX * 16;
    settings.mStartY = startY;
    settings.mStartZ = chunkZ * 16;
    settings.mProjection = structure.mProjection;
    settings.mMaxDistanceHorizontal = structure.mMaxDistanceHorizontal;
    settings.mMaxDistanceVertical = structure.mMaxDistanceVertical;
    settings.mPaddingTop = structure.mPaddingTop;
    settings.mPaddingBottom = structure.mPaddingBottom;
    settings.mDimensionMinY = LevelChunk::MIN_Y;
    settings.mDimensionHeight = LevelChunk::MAX_Y + 1;
    // any biome of the column under the start's centre will do
    if (!structure.mBiomeFilter.empty()) {
        settings.mColumnFilter = [&generator, &structure](int32_t x, int32_t z) {
            for (int32_t y = LevelChunk::MIN_Y; y <= LevelChunk::MAX_Y; y += 4) {
                if (passes(structure, generator.pickBiome(x, y, z)))
                    return true;
            }
            return false;
        };
    }

    const auto shapes = [this](const JigsawPlacer::Element &element) -> std::shared_ptr<const JigsawPlacer::Template> {
        const auto found = mPackShapes.find(element.mLocation);
        if (found != mPackShapes.end())
            return found->second;
        if (mFromPacks && element.mLocation.find(':') != std::string::npos && !element.mLocation.starts_with(NAMESPACE))
            return nullptr;
        return templateOf(element);
    };
    JigsawPlacer placer(nextInt, pools, shapes, height);
    JigsawPlacer::Result result;
    if (!placer.place(settings, result) || result.mPieces.empty())
        return nullptr;

    // the start itself must stand in a biome of the structure
    if (!passes(structure, generator.pickBiome(result.x, result.y, result.z)))
        return nullptr;

    auto plan = std::make_shared<Plan>();
    plan->mStructure = &structure;
    plan->mStartChunkX = chunkX;
    plan->mStartChunkZ = chunkZ;
    plan->mPieces = std::move(result.mPieces);
    return plan;
}

std::shared_ptr<const JigsawStructures::Plan> JigsawStructures::plan(const ChunkGenerator &generator, const Set &set,
                                                                     int32_t chunkX, int32_t chunkZ) const {
    const PlanKey key = {generator.getSeed(), &set, chunkX, chunkZ};
    {
        const std::lock_guard<std::mutex> lock(mPlanMutex);
        const auto found = mPlans.find(key);
        if (found != mPlans.end())
            return found->second;
    }

    std::shared_ptr<const Plan> planned;
    const auto floorDiv = [](int32_t value, int32_t by) {
        return value / by - (value % by < 0 ? 1 : 0);
    };
    bool isCandidate = false;
    for (size_t index = 0; index < mSets.size(); ++index) {
        if (mSets[index].get() != &set)
            continue;
        const StructureChunkPos drawn = mFeatures[index]->getCandidate(
                generator.getSeed(), floorDiv(chunkX, set.mSpacing), floorDiv(chunkZ, set.mSpacing));
        isCandidate = drawn.x == chunkX && drawn.z == chunkZ;
    }

    // TODO: a frequency below 1; no set of the game has one
    if (isCandidate && set.mFrequency >= 1.0f && generator.getDimensionType() == DimensionType::Overworld) {
        if (set.mStructures.size() == 1) {
            planned = _generate(generator, *set.mStructures.front().mStructure, chunkX, chunkZ);
        } else {
            // a structure that cannot start leaves the draw and the others are drawn again
            std::vector<WeightedStructure> left = set.mStructures;
            int32_t total = 0;
            for (const WeightedStructure &weighted: left)
                total += weighted.mWeight;
            LegacyRandom random = largeFeatureRandom(generator.getSeed(), chunkX, chunkZ);
            while (!left.empty() && total > 0) {
                int32_t draw = random.nextInt(total);
                size_t index = 0;
                for (; index < left.size(); ++index) {
                    draw -= left[index].mWeight;
                    if (draw < 0)
                        break;
                }
                index = std::min(index, left.size() - 1);
                planned = _generate(generator, *left[index].mStructure, chunkX, chunkZ);
                if (planned != nullptr)
                    break;
                total -= left[index].mWeight;
                left.erase(left.begin() + (std::ptrdiff_t) index);
            }
        }
    }

    const std::lock_guard<std::mutex> lock(mPlanMutex);
    const auto inserted = mPlans.emplace(key, planned);
    if (!inserted.second)
        return inserted.first->second;
    mPlanOrder.push_back(key);
    if (mPlanOrder.size() > CACHED_PLANS) {
        mPlans.erase(mPlanOrder.front());
        mPlanOrder.pop_front();
    }
    return planned;
}

std::vector<std::shared_ptr<const JigsawStructures::Plan>>
JigsawStructures::_plansNear(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    std::vector<std::shared_ptr<const Plan>> found;
    for (size_t index = 0; index < mSets.size(); ++index) {
        const Set &set = *mSets[index];
        int32_t distance = 0;
        for (const WeightedStructure &weighted: set.mStructures)
            distance = std::max(distance, weighted.mStructure->mMaxDistanceHorizontal);
        // the farthest piece box ends one template beyond the distance from the centre
        const int32_t reach = (distance + 15) / 16 + 2;
        for (const StructureChunkPos &start: mFeatures[index]->getCandidatesIn(
                     generator.getSeed(), chunkX - reach, chunkZ - reach, chunkX + reach, chunkZ + reach)) {
            std::shared_ptr<const Plan> planned = plan(generator, set, start.x, start.z);
            if (planned != nullptr)
                found.push_back(std::move(planned));
        }
    }
    return found;
}

void JigsawStructures::addBeard(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ,
                                TerrainBeard &beard) const {
    for (const std::shared_ptr<const Plan> &planned: _plansNear(generator, chunkX, chunkZ)) {
        const Structure &structure = *planned->mStructure;
        if (!structure.mAdapts)
            continue;

        for (const JigsawPlacer::Piece &piece: planned->mPieces) {
            if (piece.mElement->mTerrainMatching)
                continue;
            const JigsawPlacer::Box &box = piece.mBox;
            beard.addPiece(chunkX, chunkZ,
                           {box.mMinX, box.mMinY, box.mMinZ, box.mMaxX, box.mMaxY, box.mMaxZ, piece.mGroundLevelDelta,
                            structure.mAdaptation});
            for (const JigsawPlacer::Junction &junction: piece.mJunctions) {
                if (junction.mTerrainMatching)
                    beard.addJunction(chunkX, chunkZ, junction.x, junction.y, junction.z);
            }
        }
    }
}

void JigsawStructures::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk,
                             std::vector<GeneratedBlockChange> *overflow) const {
    const std::vector<std::shared_ptr<const Plan>> planned = _plansNear(generator, chunk.getX(), chunk.getZ());
    if (planned.empty())
        return;

    StructureTerrain terrain(generator, level.getTerrainCache());
    const uint32_t seed = StructureFeature::getPostProcessSeed(generator.getSeed(), chunk.getX(), chunk.getZ());
    for (const std::shared_ptr<const Plan> &structure: planned) {
        std::mt19937 chunkRandom(seed);
        ChunkShare share;
        BlockManager features(level);
        features.setOverflowSink(chunk.getX(), chunk.getZ(), overflow);
        for (const JigsawPlacer::Piece &piece: structure->mPieces) {
            if (!piece.mElement->mFeature.empty()) {
                if (insideChunk(piece.x, piece.z, chunk.getX(), chunk.getZ()))
                    OverworldRulePlacer::placeFeature(features, piece.mElement->mFeature, piece.x, piece.y, piece.z,
                                                      chunkRandom);
                continue;
            }
            if (!touchesChunk(piece.mBox, chunk.getX(), chunk.getZ()))
                continue;

            const auto packed = mPackTemplates.find(piece.mElement->mLocation);
            if (packed != mPackTemplates.end()) {
                PiecePlacer(*this, piece, chunk.getX(), chunk.getZ(), seed, chunkRandom, terrain, share)
                        .place(*packed->second, chunk);
                continue;
            }

            const std::shared_ptr<const StructureTemplate> shape =
                    StructureTemplate::load(templatePath(piece.mElement->mLocation));
            if (shape == nullptr)
                continue;
            PiecePlacer(*this, piece, chunk.getX(), chunk.getZ(), seed, chunkRandom, terrain, share).place(*shape);
        }

        // fences, panes and bars meet what stands around them
        const auto neighbour = [&](int32_t x, int32_t y, int32_t z) -> BlockState {
            const auto found = share.mBlocks.find({x, y, z});
            if (found != share.mBlocks.end())
                return found->second;
            return terrain.getBlock(x, y, z);
        };
        std::vector<std::pair<Position, BlockState>> shaped;
        for (const auto &entry: share.mBlocks)
            shaped.emplace_back(entry.first, StructureConnections::shaped(entry.second, std::get<0>(entry.first),
                                                                          std::get<1>(entry.first),
                                                                          std::get<2>(entry.first), neighbour));

        features.applySubChunkUpdate();
        for (const auto &entry: shaped)
            chunk.setBlock(std::get<0>(entry.first) & 15, std::get<1>(entry.first), std::get<2>(entry.first) & 15,
                           entry.second);

        for (const ChunkShare::Loot &loot: share.mLoot) {
            const Vector3i position(std::get<0>(loot.mPosition), std::get<1>(loot.mPosition),
                                    std::get<2>(loot.mPosition));
            const auto block = share.mBlocks.find(loot.mPosition);
            if (block == share.mBlocks.end() || block->second.mName != loot.mBlock)
                continue;

            if (loot.mBrushable) {
                BrushableBlockActor &actor = level.getBlockActors().getOrCreate<BrushableBlockActor>(position);
                actor.setType(loot.mBlock);
                actor.setLootTable(loot.mTable, loot.mSeed);
                continue;
            }

            std::unique_ptr<BlockActor> actor = containerOf(block->second);
            if (actor == nullptr)
                continue;
            actor->setPosition(position);
            actor->setLootTable(loot.mTable, loot.mSeed);
            const auto sherds = share.mSherds.find(loot.mPosition);
            auto *pot = dynamic_cast<DecoratedPotBlockActor *>(actor.get());
            if (pot != nullptr && sherds != share.mSherds.end())
                pot->setSherds(sherds->second);
            level.getBlockActors().insert(std::move(actor));
        }

        for (const std::pair<Tag, std::string> &generated: share.mGenerated) {
            const Position position = {generated.first.getInt("x", 0), generated.first.getInt("y", 0),
                                       generated.first.getInt("z", 0)};
            const auto block = share.mBlocks.find(position);
            if (block == share.mBlocks.end() || block->second.mName != generated.second)
                continue;
            if (generated.first.getString("id") == TrialSpawnerBlockActor::BLOCK_ACTOR_ID) {
                auto spawner = std::make_unique<TrialSpawnerBlockActor>();
                const auto &[x, y, z] = position;
                spawner->setPosition(Vector3i(x, y, z));
                spawner->loadTrialData(generated.first);
                level.getBlockActors().insert(std::move(spawner));
            } else {
                level.getBlockActors().keepGenerated(generated.first);
            }
        }
    }
}
