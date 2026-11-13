#include "World/Level/LevelGen/Structure/PillagerOutpostStructure.h"

#include "World/Actor/Mob/Hostile/PillagerActor.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Mob/Neutral/IronGolemActor.h"
#include "World/Actor/Mob/Passive/AllayActor.h"
#include "World/Level/Block/Actor/BannerBlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/BlockMaterial.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Structure/StructureBlockTable.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"
#include "World/Level/LevelGen/Structure/StructureTerrain.h"

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <utility>

namespace {
    const int32_t SEA_LEVEL = 63;
    // the props of a start reach three chunks away at most
    const int32_t REACH = 3;

    const float OVERGROWN_INTEGRITY = 0.05f;
    const float FEATURE_CHANCE = 0.9f;
    // a watchtower on ground uneven by more than this is sunk under its highest corner
    const int32_t MAX_UNEVENNESS = 8;
    const int32_t MAX_ALLAYS = 3;

    const char *const LOOT_TABLE = "loot_tables/chests/pillager_outpost.json";
    // the outpost start's vfunc +24 (sub_1473C53C0)
    const int32_t START_VALUE = -3;

    // sub_1473C3C20, table 0x14F2F8820
    const std::array<const char *, 7> FEATURES = {"pillageroutpost/feature_cage1",
                                                  "pillageroutpost/feature_cage2",
                                                  "pillageroutpost/feature_cage_with_allays",
                                                  "pillageroutpost/feature_logs",
                                                  "pillageroutpost/feature_targets",
                                                  "pillageroutpost/feature_tent1",
                                                  "pillageroutpost/feature_tent2"};
    // table 0x14F2F8890: one prop spot per corner around the middle of the tower area
    const std::array<std::pair<int32_t, int32_t>, 4> SPOTS = {{{16, 16}, {-16, 16}, {16, -16}, {-16, -16}}};

    // by quarter turns: how far the rotated 16 by 16 area reaches from the start corner
    const std::array<int32_t, 4> AREA_X = {15, -15, -15, 15};
    const std::array<int32_t, 4> AREA_Z = {15, 15, -15, -15};

    Tag generatedMob(const char *identifier, int32_t x, int32_t y, int32_t z) {
        // sub_1473C6A20 spawns on the block corner
        Tag location = Tag::ofList(Tag::Type::Float);
        location.addToList(Tag::ofFloat((float) x));
        location.addToList(Tag::ofFloat((float) y));
        location.addToList(Tag::ofFloat((float) z));

        Tag mob = Tag::ofCompound();
        mob.putString("identifier", identifier);
        mob.put("Pos", location);
        return mob;
    }

    using PieceKey = std::tuple<const Level *, int32_t, int32_t, size_t>;

    struct PendingMob {
        int32_t x;
        int32_t z;
        Tag mMob;
    };

    // PillagerOutpostPiece +268: the first chunk to handle data markers of a piece records its mobs; the mobs are
    // spawned by each chunk holding their spot (postProcessMobsAt, sub_1473C6A20)
    std::mutex claimsMutex;
    std::map<PieceKey, std::vector<PendingMob>> claims;

    std::string lootTablePath(const std::string &name) {
        const size_t colon = name.find(':');
        return "loot_tables/" + (colon == std::string::npos ? name : name.substr(colon + 1)) + ".json";
    }

    class PiecePlacer {
    public:
        PiecePlacer(Level &level, LevelChunk &chunk, uint32_t seed, std::mt19937 &chunkRandom,
                    StructureTerrain &terrain)
                : mLevel(level), mChunk(chunk), mSeed(seed), mChunkRandom(chunkRandom), mTerrain(terrain) {}

        // PillagerOutpostPiece::postProcess (sub_1473C5940) then the template piece post process (sub_14699DA70);
        // `box`: the box the piece is left with
        void place(const PillagerOutpostStructure::Piece &piece, const PieceKey &key, ChunkStructureBounds::Box &box) {
            const std::shared_ptr<const StructureTemplate> shape = StructureTemplate::load(piece.mTemplate + ".nbt");
            if (shape == nullptr)
                return;

            box = PillagerOutpostStructure::startBox(piece, *shape, mLevel.getMaxY() + 1);
            if (!box.intersects(ChunkStructureBounds::chunkBox(mChunk.getX(), mChunk.getZ(), mLevel.getMinY(),
                                                               mLevel.getMaxY())))
                return;

            int32_t farX = shape->getSizeX() - 1;
            int32_t farZ = shape->getSizeZ() - 1;
            StructureTemplate::rotate(piece.mQuarterTurns, farX, farZ);
            const int32_t minX = std::min(piece.x, piece.x + farX);
            const int32_t maxX = std::max(piece.x, piece.x + farX);
            const int32_t minZ = std::min(piece.z, piece.z + farZ);
            const int32_t maxZ = std::max(piece.z, piece.z + farZ);
            // a prop given up keeps the box it was made with
            int32_t originY = 0;
            if (!_ground(piece, box, originY))
                return;
            // the top is the template height over the ground, one more than the box it was made with
            box.minY = originY;
            box.maxY = originY + shape->getSizeY();
            if (maxX < mChunk.getX() * 16 || minX > mChunk.getX() * 16 + 15 || maxZ < mChunk.getZ() * 16
                || minZ > mChunk.getZ() * 16 + 15)
                return;

            // no ignored block: the air of the templates clears the ground
            std::mt19937 integrity(mSeed);
            for (const StructureTemplate::BlockEntry &entry: shape->getBlocks()) {
                const StructureTemplate::PaletteEntry &palette = shape->getPalette()[(size_t) entry.mPaletteIndex];
                if (palette.mName == "minecraft:structure_block" || palette.mName == "minecraft:jigsaw")
                    continue;
                if (piece.mIntegrity < 1.0f
                    && (double) piece.mIntegrity < (double) integrity() * 2.3283064365386963e-10)
                    continue;

                int32_t x = 0;
                int32_t y = 0;
                int32_t z = 0;
                _world(piece, entry, originY, x, y, z);
                if (!_inChunk(x, z) || y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
                    continue;

                BlockState state;
                if (!StructureBlockTable::translate(palette, piece.mQuarterTurns, state))
                    continue;

                mChunk.setBlock(x & 15, y, z & 15, state);
                const Vector3i position(x, y, z);
                if (entry.mData.getString("id", std::string()) == "minecraft:chest"
                    && state.mName == BlockNames::CHEST) {
                    // placing a chest draws its seed
                    const int32_t seed = FeatureRandom::nextInt(mChunkRandom);
                    ChestBlockActor &chest = mLevel.getBlockActors().getOrCreate<ChestBlockActor>(position);
                    const std::string table = entry.mData.getString("LootTable", std::string());
                    if (!table.empty())
                        chest.setLootTable(lootTablePath(table), seed);
                } else if (state.mName == "minecraft:wall_banner") {
                    mLevel.getBlockActors().getOrCreate<BannerBlockActor>(position).setType(
                            StructureBlockTable::bannerType(palette.mName));
                }
            }

            // the template hands over only the markers inside the chunk (sub_14775B430)
            std::vector<const StructureTemplate::BlockEntry *> markers;
            for (const StructureTemplate::BlockEntry &entry: shape->getBlocks()) {
                if (entry.mData.getString("metadata", std::string()).empty())
                    continue;
                int32_t x = 0;
                int32_t y = 0;
                int32_t z = 0;
                _world(piece, entry, originY, x, y, z);
                if (_inChunk(x, z) && y - 1 >= LevelChunk::MIN_Y && y <= LevelChunk::MAX_Y)
                    markers.push_back(&entry);
            }

            bool recordMobs = false;
            if (!markers.empty()) {
                const std::lock_guard<std::mutex> lock(claimsMutex);
                recordMobs = claims.emplace(key, std::vector<PendingMob>()).second;
            }

            std::vector<PendingMob> mobs;
            for (const StructureTemplate::BlockEntry *entry: markers)
                _marker(piece, *entry, originY, entry->mData.getString("metadata", std::string()), recordMobs, mobs);

            std::vector<Tag> spawned;
            {
                const std::lock_guard<std::mutex> lock(claimsMutex);
                const auto found = claims.find(key);
                if (found == claims.end())
                    return;
                std::vector<PendingMob> &pending = found->second;
                pending.insert(pending.end(), mobs.begin(), mobs.end());
                for (auto it = pending.begin(); it != pending.end();) {
                    if (!_inChunk(it->x, it->z)) {
                        ++it;
                        continue;
                    }
                    spawned.push_back(std::move(it->mMob));
                    it = pending.erase(it);
                }
            }
            for (Tag &mob: spawned)
                mLevel.addGeneratedEntity(std::move(mob));
        }

    private:
        bool _inChunk(int32_t x, int32_t z) const {
            return (x >> 4) == mChunk.getX() && (z >> 4) == mChunk.getZ();
        }

        static void _world(const PillagerOutpostStructure::Piece &piece, const StructureTemplate::BlockEntry &entry,
                           int32_t originY, int32_t &x, int32_t &y, int32_t &z) {
            x = entry.x;
            z = entry.z;
            StructureTemplate::rotate(piece.mQuarterTurns, x, z);
            x += piece.x;
            z += piece.z;
            y = originY + entry.y;
        }

        // the neighbour chunks are read bare: the game sees them in whatever state they are
        const BlockState &_block(int32_t x, int32_t y, int32_t z) {
            return _inChunk(x, z) ? mChunk.getBlock(x & 15, y, z & 15) : mTerrain.getBlock(x, y, z);
        }

        bool _ground(const PillagerOutpostStructure::Piece &piece, const ChunkStructureBounds::Box &box, int32_t &out) {
            const std::array<std::pair<int32_t, int32_t>, 5> points = PillagerOutpostStructure::groundPoints(box);
            std::array<int32_t, 5> heights = {};
            for (size_t i = 0; i < points.size(); ++i)
                heights[i] = mTerrain.getTopHeight(points[i].first, points[i].second);

            const int32_t highest = *std::max_element(heights.begin(), heights.end());
            const int32_t floor = std::max(*std::min_element(heights.begin(), heights.end()), SEA_LEVEL);
            if (piece.mFeature) {
                // the piece box still starts at sea level: the block under it is tested
                for (const std::pair<int32_t, int32_t> &point: points) {
                    if ((BlockMaterial::flagsOf(_block(point.first, box.minY - 1, point.second))
                         & BlockMaterial::FLAG_LIQUID)
                        != 0)
                        return false;
                }
                if (highest - floor > MAX_UNEVENNESS)
                    return false;
                out = heights[0];
                return true;
            }

            out = highest - floor <= MAX_UNEVENNESS ? (highest + floor) / 2 : highest - 4;
            return true;
        }

        // PillagerOutpostPiece::handleDataMarker (sub_1473C5ED0) and postProcessMobsAt (sub_1473C6620)
        void _marker(const PillagerOutpostStructure::Piece &piece, const StructureTemplate::BlockEntry &entry,
                     int32_t originY, const std::string &marker, bool recordMobs, std::vector<PendingMob> &mobs) {
            int32_t x = 0;
            int32_t y = 0;
            int32_t z = 0;
            _world(piece, entry, originY, x, y, z);

            // a piece already handled by an earlier chunk skips straight to the chest marker
            if (recordMobs) {
                const BlockState air(BlockNames::AIR);
                if (marker.compare(0, 4, "cage") == 0) {
                    mChunk.setBlock(x & 15, y, z & 15, air);
                    // the game moves the golem one block east for this turn only
                    const int32_t golemX = piece.mQuarterTurns == 1 ? x + 1 : x;
                    mobs.push_back({golemX, z, generatedMob(IronGolemActor::IDENTIFIER, golemX, y, z)});
                } else if (marker.compare(0, 8, "pillager") == 0) {
                    mChunk.setBlock(x & 15, y, z & 15, air);
                    mobs.push_back({x, z, generatedMob(PillagerActor::IDENTIFIER, x, y, z)});
                } else if (marker.compare(0, 7, "captain") == 0) {
                    mChunk.setBlock(x & 15, y, z & 15, air);
                    Tag captain = generatedMob(PillagerActor::IDENTIFIER, x, y, z);
                    captain.putString(MobActor::TAG_SPAWN_EVENT, "minecraft:spawn_as_illager_captain");
                    mobs.push_back({x, z, captain});
                } else if (marker == "Group of Allays") {
                    mChunk.setBlock(x & 15, y, z & 15, air);
                    const int32_t count = FeatureRandom::nextInt(mChunkRandom, MAX_ALLAYS) + 1;
                    for (int32_t i = 0; i < count; ++i)
                        mobs.push_back({x, z, generatedMob(AllayActor::IDENTIFIER, x, y, z)});
                }
            }

            if (marker.compare(0, 8, "topChest") != 0)
                return;

            ChestBlockActor *chest = mLevel.getBlockActors().find<ChestBlockActor>(Vector3i(x, y - 1, z));
            if (chest != nullptr)
                chest->setLootTable(LOOT_TABLE, FeatureRandom::nextInt(mChunkRandom));
        }

        Level &mLevel;
        LevelChunk &mChunk;
        uint32_t mSeed;
        std::mt19937 &mChunkRandom;
        StructureTerrain &mTerrain;
    };
}

std::vector<PillagerOutpostStructure::Piece> PillagerOutpostStructure::plan(const ChunkGenerator &generator,
                                                                            int32_t startX, int32_t startZ) {
    // createStart (sub_1473C3C20) goes on with the random isFeatureChunk drew the start from
    std::mt19937 random =
            StructureFeature::get("pillager_outpost")->createStartRandom(generator.getSeed(), startX, startZ);
    const int32_t turns = FeatureRandom::nextInt(random, 4);
    const int32_t x = startX * 16;
    const int32_t z = startZ * 16;

    std::vector<Piece> pieces;
    pieces.push_back({"pillageroutpost/watchtower", x, z, turns, 1.0f, false});
    pieces.push_back({"pillageroutpost/watchtower_overgrown", x, z, turns, OVERGROWN_INTEGRITY, false});

    const int32_t middleX = std::min(x, x + AREA_X[(size_t) turns]) + 8;
    const int32_t middleZ = std::min(z, z + AREA_Z[(size_t) turns]) + 8;
    std::array<std::pair<int32_t, int32_t>, 4> spots = {};
    for (size_t i = 0; i < SPOTS.size(); ++i) {
        const int32_t spotX = FeatureRandom::nextInt(random, 8) + SPOTS[i].first - 4;
        const int32_t spotZ = SPOTS[i].second + FeatureRandom::nextInt(random, 8) - 4;
        spots[i] = {spotX, spotZ};
    }

    for (const std::pair<int32_t, int32_t> &spot: spots) {
        if (FeatureRandom::nextFloat(random) >= FEATURE_CHANCE)
            continue;
        const int32_t featureTurns = FeatureRandom::nextInt(random, 4);
        const char *name = FEATURES[(size_t) FeatureRandom::nextInt(random, (int32_t) FEATURES.size())];
        pieces.push_back({name, middleX + spot.first, middleZ + spot.second, featureTurns, 1.0f, true});
    }

    return pieces;
}

void PillagerOutpostStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    const StructureFeature *outposts = StructureFeature::get("pillager_outpost");
    std::vector<std::pair<PieceKey, Piece>> pieces;
    for (int32_t startX = chunk.getX() - REACH; startX <= chunk.getX() + REACH; ++startX) {
        for (int32_t startZ = chunk.getZ() - REACH; startZ <= chunk.getZ() + REACH; ++startZ) {
            if (!outposts->isStartChunk(generator, startX, startZ))
                continue;
            const std::vector<Piece> start = plan(generator, startX, startZ);
            for (size_t i = 0; i < start.size(); ++i)
                pieces.emplace_back(PieceKey(&level, startX, startZ, i), start[i]);
        }
    }
    if (pieces.empty())
        return;

    const uint32_t seed = StructureFeature::getPostProcessSeed(generator.getSeed(), chunk.getX(), chunk.getZ());
    std::mt19937 chunkRandom(seed);
    StructureTerrain terrain(generator);
    PiecePlacer placer(level, chunk, seed, chunkRandom, terrain);
    std::vector<ChunkStructureBounds::Box> boxes;
    for (const std::pair<PieceKey, Piece> &piece: pieces) {
        ChunkStructureBounds::Box box;
        placer.place(piece.second, piece.first, box);
        boxes.push_back(box);
    }

    // registered once the chunk's pieces are done (sub_14CDACCA0), one start at a time
    const int32_t top = level.getMaxY() + 1;
    const ChunkStructureBounds::Box chunkBox = ChunkStructureBounds::chunkBox(chunk.getX(), chunk.getZ(),
                                                                              level.getMinY(), level.getMaxY());
    for (size_t first = 0; first < pieces.size();) {
        size_t last = first;
        ChunkStructureBounds::Box start;
        bool any = false;
        std::vector<ChunkStructureBounds::Box> pieceBoxes;
        for (; last < pieces.size() && std::get<1>(pieces[last].first) == std::get<1>(pieces[first].first)
               && std::get<2>(pieces[last].first) == std::get<2>(pieces[first].first);
             ++last) {
            const std::shared_ptr<const StructureTemplate> shape = StructureTemplate::load(pieces[last].second.mTemplate
                                                                                           + ".nbt");
            if (shape == nullptr)
                continue;
            // StructureStart::calculateBoundingBox over the boxes the pieces were made with
            const ChunkStructureBounds::Box made = startBox(pieces[last].second, *shape, top);
            start = any ? ChunkStructureBounds::Box{std::min(start.minX, made.minX), std::min(start.minY, made.minY),
                                                    std::min(start.minZ, made.minZ), std::max(start.maxX, made.maxX),
                                                    std::max(start.maxY, made.maxY), std::max(start.maxZ, made.maxZ)}
                        : made;
            any = true;
            pieceBoxes.push_back(boxes[last]);
        }
        if (any)
            chunk.getStructureBounds().addStart("minecraft:pillager_outpost", START_VALUE, start, pieceBoxes, chunkBox);
        first = last;
    }
}

// PillagerOutpostPiece::postProcess (sub_1473C5940, 26.60) samples the box the piece was made with: one block wider
// than the template, on its far side
std::array<std::pair<int32_t, int32_t>, 5>
PillagerOutpostStructure::groundPoints(const ChunkStructureBounds::Box &box) {
    const int32_t halfX = (box.maxX - box.minX + 1) >> 1;
    const int32_t halfZ = (box.maxZ - box.minZ + 1) >> 1;
    return {{{box.minX + halfX, box.minZ + halfZ},
             {box.minX + 2 * halfX, box.minZ + halfZ},
             {box.minX + halfX, box.minZ + 2 * halfZ},
             {box.minX, box.minZ + halfZ},
             {box.minX + halfX, box.minZ}}};
}

// TemplateStructurePiece::calculateBoundingBox (sub_14699D910, 26.60): the far sides reach one block past the
// template; the outpost piece then spans from the sea to the top of the dimension (ctor sub_1473C53F0)
ChunkStructureBounds::Box PillagerOutpostStructure::startBox(const Piece &piece, const StructureTemplate &shape,
                                                             int32_t top) {
    const int32_t sizeX = shape.getSizeX();
    const int32_t sizeZ = shape.getSizeZ();
    ChunkStructureBounds::Box box;
    box.minX = piece.x;
    box.maxX = piece.x;
    box.minZ = piece.z;
    box.maxZ = piece.z;
    switch (piece.mQuarterTurns & 3) {
        case 1:
            box.minX -= sizeZ;
            box.maxZ += sizeX;
            break;
        case 2:
            box.minX -= sizeX;
            box.minZ -= sizeZ;
            break;
        case 3:
            box.maxX += sizeZ;
            box.minZ -= sizeX;
            break;
        default:
            box.maxX += sizeX;
            box.maxZ += sizeZ;
            break;
    }
    box.minY = SEA_LEVEL;
    box.maxY = top;
    return box;
}
