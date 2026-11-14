#include "World/Level/LevelGen/Structure/NetherFortressStructure.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/ChunkStructureBounds.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Structure/StructurePieceWriter.h"
#include "World/Level/LevelGen/Nether/NetherGeneratorConstants.h"
#include "World/Level/LevelGen/Structure/NetherFortressFeature.h"

#include <algorithm>
#include <deque>
#include <limits>
#include <map>
#include <mutex>
#include <tuple>

namespace {
    const size_t CACHED_PLANS = 64;

    // the door of a piece stays within 112 blocks of the start corner, the piece itself up to 19 further
    const int32_t PIECE_REACH = NetherFortressPieces::MAX_DISTANCE + 19 + 2;

    const NetherFortressFeature *feature() {
        return dynamic_cast<const NetherFortressFeature *>(StructureFeature::get("fortress"));
    }

    StructurePiece::Box boxAround(const std::vector<NetherFortressPieces::Piece> &pieces) {
        StructurePiece::Box box;
        box.mMinX = box.mMinY = box.mMinZ = std::numeric_limits<int32_t>::max();
        box.mMaxX = box.mMaxY = box.mMaxZ = std::numeric_limits<int32_t>::min();
        for (const NetherFortressPieces::Piece &piece: pieces) {
            const StructurePiece::Box &other = piece.getBox();
            box.mMinX = std::min(box.mMinX, other.mMinX);
            box.mMinY = std::min(box.mMinY, other.mMinY);
            box.mMinZ = std::min(box.mMinZ, other.mMinZ);
            box.mMaxX = std::max(box.mMaxX, other.mMaxX);
            box.mMaxY = std::max(box.mMaxY, other.mMaxY);
            box.mMaxZ = std::max(box.mMaxZ, other.mMaxZ);
        }
        return box;
    }
}

// the dimension's sea level, which the game keeps one above the lava level
const int32_t NetherFortressStructure::SEA_LEVEL = NetherGeneratorConstants::LAVA_LEVEL + 1;

const int32_t NetherFortressStructure::REACH = (PIECE_REACH + 15) / 16;

std::shared_ptr<const NetherFortressStructure::Plan> NetherFortressStructure::plan(const ChunkGenerator &generator,
                                                                                   int32_t startX, int32_t startZ) {
    // the plan reads only the seed: every worker's generator shares it
    using Key = std::tuple<int64_t, int32_t, int32_t>;
    static std::mutex mutex;
    static std::map<Key, std::shared_ptr<const Plan>> plans;
    static std::deque<Key> order;

    const Key key = {generator.getSeed(), startX, startZ};
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = plans.find(key);
        if (found != plans.end())
            return found->second;
    }

    const NetherFortressFeature *fortress = feature();
    if (fortress == nullptr || !fortress->isStartChunk(generator, startX, startZ))
        return nullptr;

    // planned unlocked: other workers keep planning
    std::mt19937 random = fortress->createFortressRandom(generator.getSeed(), startX, startZ);
    auto made = std::make_shared<Plan>();
    made->mStartX = startX;
    made->mStartZ = startZ;
    made->mPieces = NetherFortressPieces::assemble(random, startX, startZ, SEA_LEVEL);
    made->mBox = boxAround(made->mPieces);
    std::shared_ptr<const Plan> planned = std::move(made);

    std::lock_guard<std::mutex> lock(mutex);
    const auto inserted = plans.emplace(key, planned);
    if (!inserted.second)
        return inserted.first->second;

    order.push_back(key);
    if (order.size() > CACHED_PLANS) {
        plans.erase(order.front());
        order.pop_front();
    }
    return planned;
}

std::vector<std::shared_ptr<const NetherFortressStructure::Plan>>
NetherFortressStructure::plansNear(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) {
    std::vector<std::shared_ptr<const Plan>> found;
    const NetherFortressFeature *fortress = feature();
    if (fortress == nullptr || generator.getDimensionType() != fortress->getDimension())
        return found;

    const int32_t minX = chunkX * 16;
    const int32_t minZ = chunkZ * 16;
    for (const StructureChunkPos &start: fortress->getCandidatesIn(generator.getSeed(), chunkX - REACH,
                                                                   chunkZ - REACH, chunkX + REACH, chunkZ + REACH)) {
        std::shared_ptr<const Plan> pieces = plan(generator, start.x, start.z);
        if (pieces == nullptr)
            continue;

        const StructurePiece::Box &box = pieces->mBox;
        if (box.mMaxX >= minX && box.mMinX <= minX + 15 && box.mMaxZ >= minZ && box.mMinZ <= minZ + 15)
            found.push_back(std::move(pieces));
    }
    return found;
}

namespace {
    using Piece = NetherFortressPieces::Piece;
    using Type = NetherFortressPieces::Type;

    const char *const BLAZE = "minecraft:blaze";
    const char *const FORTRESS_NAME = "minecraft:fortress";

    // chests face west whatever the piece's turn: their data skips the rotation
    const int32_t CHEST_DIRECTION = 4;

    struct Blocks {
        BlockState mBrick = VanillaBlocks::NETHER_BRICK().toBlockState();
        BlockState mFence = VanillaBlocks::NETHER_BRICK_FENCE().toBlockState();
        BlockState mAir = VanillaBlocks::AIR().toBlockState();
        BlockState mStairs = VanillaBlocks::NETHER_BRICK_STAIRS().toBlockState();
        BlockState mWart = VanillaBlocks::NETHER_WART().toBlockState();
        BlockState mSoulSand = VanillaBlocks::SOUL_SAND().toBlockState();
        BlockState mLava = VanillaBlocks::LAVA().toBlockState();
    };

    const Blocks &blocks() {
        static const Blocks shared;
        return shared;
    }

    class Builder {
    public:
        Builder(const Piece &piece, Level &level, LevelChunk &chunk) : mPiece(piece), mWriter(level, chunk, piece) {}

        void box(int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY, int32_t maxZ,
                 const BlockState &state) {
            mWriter.generateBox(minX, minY, minZ, maxX, maxY, maxZ, state, state, false);
        }

        void box(int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY, int32_t maxZ,
                 const BlockState &edge, const BlockState &inside) {
            mWriter.generateBox(minX, minY, minZ, maxX, maxY, maxZ, edge, inside, false);
        }

        void place(const BlockState &state, int32_t x, int32_t y, int32_t z) {
            mWriter.placeBlock(state, x, y, z);
        }

        void column(int32_t x, int32_t z) {
            mWriter.fillColumnDown(blocks().mBrick, x, -1, z);
        }

        BlockState stairs(int32_t direction) const {
            return DecorationSupport::withState(blocks().mStairs, "weirdo_direction",
                                                mPiece.stairsDirection(direction));
        }

        StructurePieceWriter &writer() {
            return mWriter;
        }

        const Piece &piece() const {
            return mPiece;
        }

    private:
        const Piece &mPiece;
        StructurePieceWriter mWriter;
    };

    void bridgeCrossing(Builder &b) {
        const Blocks &k = blocks();
        b.box(7, 3, 0, 11, 4, 18, k.mBrick);
        b.box(0, 3, 7, 18, 4, 11, k.mBrick);
        b.box(8, 5, 0, 10, 7, 18, k.mAir);
        b.box(0, 5, 8, 18, 7, 10, k.mAir);
        b.box(7, 5, 0, 7, 5, 7, k.mBrick);
        b.box(7, 5, 11, 7, 5, 18, k.mBrick);
        b.box(11, 5, 0, 11, 5, 7, k.mBrick);
        b.box(11, 5, 11, 11, 5, 18, k.mBrick);
        b.box(0, 5, 7, 7, 5, 7, k.mBrick);
        b.box(11, 5, 7, 18, 5, 7, k.mBrick);
        b.box(0, 5, 11, 7, 5, 11, k.mBrick);
        b.box(11, 5, 11, 18, 5, 11, k.mBrick);
        b.box(7, 2, 0, 11, 2, 5, k.mBrick);
        b.box(7, 2, 13, 11, 2, 18, k.mBrick);
        b.box(7, 0, 0, 11, 1, 3, k.mBrick);
        b.box(7, 0, 15, 11, 1, 18, k.mBrick);
        for (int32_t x = 7; x <= 11; ++x) {
            for (int32_t z = 0; z <= 2; ++z) {
                b.column(x, z);
                b.column(x, 18 - z);
            }
        }
        b.box(0, 2, 7, 5, 2, 11, k.mBrick);
        b.box(13, 2, 7, 18, 2, 11, k.mBrick);
        b.box(0, 0, 7, 3, 1, 11, k.mBrick);
        b.box(15, 0, 7, 18, 1, 11, k.mBrick);
        for (int32_t x = 0; x <= 2; ++x) {
            for (int32_t z = 7; z <= 11; ++z) {
                b.column(x, z);
                b.column(18 - x, z);
            }
        }
    }

    void bridgeStraight(Builder &b) {
        const Blocks &k = blocks();
        b.box(0, 3, 0, 4, 4, 18, k.mBrick);
        b.box(1, 5, 0, 3, 7, 18, k.mAir);
        b.box(0, 5, 0, 0, 5, 18, k.mBrick);
        b.box(4, 5, 0, 4, 5, 18, k.mBrick);
        b.box(0, 2, 0, 4, 2, 5, k.mBrick);
        b.box(0, 2, 13, 4, 2, 18, k.mBrick);
        b.box(0, 0, 0, 4, 1, 3, k.mBrick);
        b.box(0, 0, 15, 4, 1, 18, k.mBrick);
        for (int32_t x = 0; x <= 4; ++x) {
            for (int32_t z = 0; z <= 2; ++z) {
                b.column(x, z);
                b.column(x, 18 - z);
            }
        }
        b.box(0, 1, 1, 0, 4, 1, k.mFence);
        b.box(0, 3, 4, 0, 4, 4, k.mFence);
        b.box(0, 3, 14, 0, 4, 14, k.mFence);
        b.box(0, 1, 17, 0, 4, 17, k.mFence);
        b.box(4, 1, 1, 4, 4, 1, k.mFence);
        b.box(4, 3, 4, 4, 4, 4, k.mFence);
        b.box(4, 3, 14, 4, 4, 14, k.mFence);
        b.box(4, 1, 17, 4, 4, 17, k.mFence);
    }

    void bridgeEndFiller(Builder &b) {
        const Blocks &k = blocks();
        // the filler's own random: its edges crumble the same way in every chunk
        std::mt19937 random((uint32_t) b.piece().mSelfSeed);
        for (int32_t x = 0; x <= 4; ++x) {
            b.box(x, 3, 0, x, 3, FeatureRandom::nextInt(random, 8), k.mBrick);
            b.box(x, 4, 0, x, 4, FeatureRandom::nextInt(random, 8), k.mBrick);
        }
        b.box(0, 5, 0, 0, 5, FeatureRandom::nextInt(random, 8), k.mBrick);
        b.box(4, 5, 0, 4, 5, FeatureRandom::nextInt(random, 8), k.mBrick);
        for (int32_t x = 0; x <= 4; ++x)
            b.box(x, 2, 0, x, 2, FeatureRandom::nextInt(random, 5), k.mBrick);
        for (int32_t x = 0; x <= 4; ++x) {
            b.box(x, 0, 0, x, 0, FeatureRandom::nextInt(random, 3), k.mBrick);
            b.box(x, 1, 0, x, 1, FeatureRandom::nextInt(random, 3), k.mBrick);
        }
    }

    void roomFloorColumns(Builder &b) {
        for (int32_t x = 0; x <= 6; ++x) {
            for (int32_t z = 0; z <= 6; ++z)
                b.column(x, z);
        }
    }

    void roomCrossing(Builder &b) {
        const Blocks &k = blocks();
        b.box(0, 0, 0, 6, 1, 6, k.mBrick);
        b.box(0, 2, 0, 6, 7, 6, k.mAir);
        b.box(0, 2, 0, 1, 6, 0, k.mBrick);
        b.box(0, 2, 6, 1, 6, 6, k.mBrick);
        b.box(5, 2, 0, 6, 6, 0, k.mBrick);
        b.box(5, 2, 6, 6, 6, 6, k.mBrick);
        b.box(0, 2, 0, 0, 6, 1, k.mBrick);
        b.box(0, 2, 5, 0, 6, 6, k.mBrick);
        b.box(6, 2, 0, 6, 6, 1, k.mBrick);
        b.box(6, 2, 5, 6, 6, 6, k.mBrick);
        b.box(2, 6, 0, 4, 6, 0, k.mBrick);
        b.box(2, 5, 0, 4, 5, 0, k.mFence);
        b.box(2, 6, 6, 4, 6, 6, k.mBrick);
        b.box(2, 5, 6, 4, 5, 6, k.mFence);
        b.box(0, 6, 2, 0, 6, 4, k.mBrick);
        b.box(0, 5, 2, 0, 5, 4, k.mFence);
        b.box(6, 6, 2, 6, 6, 4, k.mBrick);
        b.box(6, 5, 2, 6, 5, 4, k.mFence);
        roomFloorColumns(b);
    }

    void stairsRoom(Builder &b) {
        const Blocks &k = blocks();
        b.box(0, 0, 0, 6, 1, 6, k.mBrick);
        b.box(0, 2, 0, 6, 10, 6, k.mAir);
        b.box(0, 2, 0, 1, 8, 0, k.mBrick);
        b.box(5, 2, 0, 6, 8, 0, k.mBrick);
        b.box(0, 2, 1, 0, 8, 6, k.mBrick);
        b.box(6, 2, 1, 6, 8, 6, k.mBrick);
        b.box(1, 2, 6, 5, 8, 6, k.mBrick);
        b.box(0, 3, 2, 0, 5, 4, k.mFence);
        b.box(6, 3, 2, 6, 5, 2, k.mFence);
        b.box(6, 3, 4, 6, 5, 4, k.mFence);
        b.place(k.mBrick, 5, 2, 5);
        b.box(4, 2, 5, 4, 3, 5, k.mBrick);
        b.box(3, 2, 5, 3, 4, 5, k.mBrick);
        b.box(2, 2, 5, 2, 5, 5, k.mBrick);
        b.box(1, 2, 5, 1, 6, 5, k.mBrick);
        b.box(1, 7, 1, 5, 7, 4, k.mBrick);
        b.box(6, 8, 2, 6, 8, 4, k.mAir);
        b.box(2, 6, 0, 4, 8, 0, k.mBrick);
        b.box(2, 5, 0, 4, 5, 0, k.mFence);
        roomFloorColumns(b);
    }

    void monsterThrone(Builder &b) {
        const Blocks &k = blocks();
        b.box(0, 2, 0, 6, 7, 7, k.mAir);
        b.box(1, 0, 0, 5, 1, 7, k.mBrick);
        b.box(1, 2, 1, 5, 2, 7, k.mBrick);
        b.box(1, 3, 2, 5, 3, 7, k.mBrick);
        b.box(1, 4, 3, 5, 4, 7, k.mBrick);
        b.box(1, 2, 0, 1, 4, 2, k.mBrick);
        b.box(5, 2, 0, 5, 4, 2, k.mBrick);
        b.box(1, 5, 2, 1, 5, 3, k.mBrick);
        b.box(5, 5, 2, 5, 5, 3, k.mBrick);
        b.box(0, 5, 3, 0, 5, 8, k.mBrick);
        b.box(6, 5, 3, 6, 5, 8, k.mBrick);
        b.box(1, 5, 8, 5, 5, 8, k.mBrick);
        b.place(k.mFence, 1, 6, 3);
        b.place(k.mFence, 5, 6, 3);
        b.box(0, 6, 3, 0, 6, 8, k.mFence);
        b.box(6, 6, 3, 6, 6, 8, k.mFence);
        b.box(1, 6, 8, 5, 7, 8, k.mFence);
        b.box(2, 8, 8, 4, 8, 8, k.mFence);
        b.writer().createSpawner(3, 5, 5, BLAZE);
        roomFloorColumns(b);
    }

    // the walls and roof the entrance and the stalk room share
    void castleShell(Builder &b) {
        const Blocks &k = blocks();
        b.box(0, 3, 0, 12, 4, 12, k.mBrick);
        b.box(0, 5, 0, 12, 13, 12, k.mAir);
        b.box(0, 5, 0, 1, 12, 12, k.mBrick);
        b.box(11, 5, 0, 12, 12, 12, k.mBrick);
        b.box(2, 5, 11, 4, 12, 12, k.mBrick);
        b.box(8, 5, 11, 10, 12, 12, k.mBrick);
        b.box(5, 9, 11, 7, 12, 12, k.mBrick);
        b.box(2, 5, 0, 4, 12, 1, k.mBrick);
        b.box(8, 5, 0, 10, 12, 1, k.mBrick);
        b.box(5, 9, 0, 7, 12, 1, k.mBrick);
        b.box(2, 11, 2, 10, 12, 10, k.mBrick);
    }

    // the battlements: the game also fences the corner a step past the last merlon
    void castleBattlements(Builder &b) {
        const Blocks &k = blocks();
        for (int32_t i = 1; i <= 11; i += 2) {
            b.box(i, 10, 0, i, 11, 0, k.mFence);
            b.box(i, 10, 12, i, 11, 12, k.mFence);
            b.box(0, 10, i, 0, 11, i, k.mFence);
            b.box(12, 10, i, 12, 11, i, k.mFence);
            b.place(k.mBrick, i, 13, 0);
            b.place(k.mBrick, i, 13, 12);
            b.place(k.mBrick, 0, 13, i);
            b.place(k.mBrick, 12, 13, i);
            b.place(k.mFence, i + 1, 13, 0);
            b.place(k.mFence, i + 1, 13, 12);
            b.place(k.mFence, 0, 13, i + 1);
            b.place(k.mFence, 12, 13, i + 1);
        }
        b.place(k.mFence, 0, 13, 0);
        b.place(k.mFence, 0, 13, 12);
        b.place(k.mFence, 0, 13, 0);
        b.place(k.mFence, 12, 13, 0);
        for (int32_t z = 3; z <= 9; z += 2) {
            b.box(1, 7, z, 1, 8, z, k.mFence);
            b.box(11, 7, z, 11, 8, z, k.mFence);
        }
    }

    // the cross shaped foundation under both castle halls
    void castleFoundation(Builder &b) {
        const Blocks &k = blocks();
        b.box(4, 2, 0, 8, 2, 12, k.mBrick);
        b.box(0, 2, 4, 12, 2, 8, k.mBrick);
        b.box(4, 0, 0, 8, 1, 3, k.mBrick);
        b.box(4, 0, 9, 8, 1, 12, k.mBrick);
        b.box(0, 0, 4, 3, 1, 8, k.mBrick);
        b.box(9, 0, 4, 12, 1, 8, k.mBrick);
        for (int32_t x = 4; x <= 8; ++x) {
            for (int32_t z = 0; z <= 2; ++z) {
                b.column(x, z);
                b.column(x, 12 - z);
            }
        }
        for (int32_t x = 0; x <= 2; ++x) {
            for (int32_t z = 4; z <= 8; ++z) {
                b.column(x, z);
                b.column(12 - x, z);
            }
        }
    }

    void castleEntrance(Builder &b) {
        const Blocks &k = blocks();
        castleShell(b);
        b.box(5, 8, 0, 7, 8, 0, k.mFence);
        castleBattlements(b);
        castleFoundation(b);
        b.box(5, 5, 5, 7, 5, 7, k.mBrick);
        b.box(6, 1, 6, 6, 4, 6, k.mAir);
        b.place(k.mBrick, 6, 0, 6);
        b.place(k.mLava, 6, 5, 6);
    }

    void castleStalkRoom(Builder &b) {
        const Blocks &k = blocks();
        castleShell(b);
        castleBattlements(b);

        const BlockState up = b.stairs(3);
        for (int32_t i = 0; i <= 6; ++i) {
            for (int32_t x = 5; x <= 7; ++x)
                b.place(up, x, 5 + i, 4 + i);
            if (i >= 1 && i <= 4)
                b.box(5, 5, i + 4, 7, i + 4, i + 4, k.mBrick);
            else if (i >= 5)
                b.box(5, 8, i + 4, 7, i + 4, i + 4, k.mBrick);
            if (i >= 1)
                b.box(5, i + 6, i + 4, 7, i + 9, i + 4, k.mAir);
        }
        for (int32_t x = 5; x <= 7; ++x)
            b.place(up, x, 12, 11);
        b.box(5, 6, 7, 5, 7, 7, k.mFence);
        b.box(7, 6, 7, 7, 7, 7, k.mFence);
        b.box(5, 13, 12, 7, 13, 12, k.mAir);
        b.box(2, 5, 2, 3, 5, 3, k.mBrick);
        b.box(2, 5, 9, 3, 5, 10, k.mBrick);
        b.box(2, 5, 4, 2, 5, 8, k.mBrick);
        b.box(9, 5, 2, 10, 5, 3, k.mBrick);
        b.box(9, 5, 9, 10, 5, 10, k.mBrick);
        b.box(10, 5, 4, 10, 5, 8, k.mBrick);

        const BlockState east = b.stairs(0);
        const BlockState west = b.stairs(1);
        b.place(west, 4, 5, 2);
        b.place(west, 4, 5, 3);
        b.place(west, 4, 5, 9);
        b.place(west, 4, 5, 10);
        b.place(east, 8, 5, 2);
        b.place(east, 8, 5, 3);
        b.place(east, 8, 5, 9);
        b.place(east, 8, 5, 10);
        b.box(3, 4, 4, 4, 4, 8, k.mSoulSand);
        b.box(8, 4, 4, 9, 4, 8, k.mSoulSand);
        b.box(3, 5, 4, 4, 5, 8, k.mWart);
        b.box(8, 5, 4, 9, 5, 8, k.mWart);
        castleFoundation(b);
    }

    void corridorColumns(Builder &b) {
        for (int32_t x = 0; x <= 4; ++x) {
            for (int32_t z = 0; z <= 4; ++z)
                b.column(x, z);
        }
    }

    void smallCorridor(Builder &b) {
        const Blocks &k = blocks();
        b.box(0, 0, 0, 4, 1, 4, k.mBrick);
        b.box(0, 2, 0, 4, 5, 4, k.mAir);
        b.box(0, 2, 0, 0, 5, 4, k.mBrick);
        b.box(4, 2, 0, 4, 5, 4, k.mBrick);
        b.box(0, 3, 1, 0, 4, 1, k.mFence);
        b.box(0, 3, 3, 0, 4, 3, k.mFence);
        b.box(4, 3, 1, 4, 4, 1, k.mFence);
        b.box(4, 3, 3, 4, 4, 3, k.mFence);
        b.box(0, 6, 0, 4, 6, 4, k.mBrick);
        corridorColumns(b);
    }

    void corridorCrossing(Builder &b) {
        const Blocks &k = blocks();
        b.box(0, 0, 0, 4, 1, 4, k.mBrick);
        b.box(0, 2, 0, 4, 5, 4, k.mAir);
        b.box(0, 2, 0, 0, 5, 0, k.mBrick);
        b.box(4, 2, 0, 4, 5, 0, k.mBrick);
        b.box(0, 2, 4, 0, 5, 4, k.mBrick);
        b.box(4, 2, 4, 4, 5, 4, k.mBrick);
        b.box(0, 6, 0, 4, 6, 4, k.mBrick);
        corridorColumns(b);
    }

    void corridorTurn(Builder &b, std::mt19937 &random, bool right) {
        const Blocks &k = blocks();
        const int32_t wallX = right ? 0 : 4;
        b.box(0, 0, 0, 4, 1, 4, k.mBrick);
        b.box(0, 2, 0, 4, 5, 4, k.mAir);
        b.box(wallX, 2, 0, wallX, 5, 4, k.mBrick);
        b.box(wallX, 3, 1, wallX, 4, 1, k.mFence);
        b.box(wallX, 3, 3, wallX, 4, 3, k.mFence);
        if (right) {
            b.box(4, 2, 0, 4, 5, 0, k.mBrick);
            b.box(1, 2, 4, 4, 5, 4, k.mBrick);
        } else {
            b.box(0, 2, 0, 0, 5, 0, k.mBrick);
            b.box(0, 2, 4, 3, 5, 4, k.mBrick);
        }
        b.box(1, 3, 4, 1, 4, 4, k.mFence, k.mBrick);
        b.box(3, 3, 4, 3, 4, 4, k.mFence, k.mBrick);
        if (b.piece().mNeedsChest)
            b.writer().createChest(random, right ? 1 : 3, 2, 3, CHEST_DIRECTION, NetherFortressStructure::LOOT_TABLE);
        b.box(0, 6, 0, 4, 6, 4, k.mBrick);
        corridorColumns(b);
    }

    void corridorStairs(Builder &b) {
        const Blocks &k = blocks();
        const BlockState up = b.stairs(2);
        for (int32_t z = 0; z <= 9; ++z) {
            const int32_t floor = std::max(1, 7 - z);
            const int32_t roof = std::min(std::max(floor + 5, 14 - z), 13);
            b.box(0, 0, z, 4, floor, z, k.mBrick);
            b.box(1, floor + 1, z, 3, roof - 1, z, k.mAir);
            if (z <= 6) {
                b.place(up, 1, floor + 1, z);
                b.place(up, 2, floor + 1, z);
                b.place(up, 3, floor + 1, z);
            }
            b.box(0, roof, z, 4, roof, z, k.mBrick);
            b.box(0, floor + 1, z, 0, roof - 1, z, k.mBrick);
            b.box(4, floor + 1, z, 4, roof - 1, z, k.mBrick);
            if ((z & 1) == 0) {
                b.box(0, floor + 2, z, 0, floor + 3, z, k.mFence);
                b.box(4, floor + 2, z, 4, floor + 3, z, k.mFence);
            }
            for (int32_t x = 0; x <= 4; ++x)
                b.column(x, z);
        }
    }

    void corridorBalcony(Builder &b) {
        const Blocks &k = blocks();
        b.box(0, 0, 0, 8, 1, 8, k.mBrick);
        b.box(0, 2, 0, 8, 5, 8, k.mAir);
        b.box(0, 6, 0, 8, 6, 5, k.mBrick);
        b.box(0, 2, 0, 2, 5, 0, k.mBrick);
        b.box(6, 2, 0, 8, 5, 0, k.mBrick);
        b.box(1, 3, 0, 1, 4, 0, k.mFence);
        b.box(7, 3, 0, 7, 4, 0, k.mFence);
        b.box(0, 2, 4, 8, 2, 8, k.mBrick);
        b.box(1, 1, 4, 2, 2, 4, k.mAir);
        b.box(6, 1, 4, 7, 2, 4, k.mAir);
        b.box(0, 3, 8, 8, 3, 8, k.mFence);
        b.box(0, 3, 6, 0, 3, 7, k.mFence);
        b.box(8, 3, 6, 8, 3, 7, k.mFence);
        b.box(0, 3, 4, 0, 5, 5, k.mBrick);
        b.box(8, 3, 4, 8, 5, 5, k.mBrick);
        b.box(1, 3, 5, 2, 5, 5, k.mBrick);
        b.box(6, 3, 5, 7, 5, 5, k.mBrick);
        b.box(1, 4, 5, 1, 5, 5, k.mFence);
        b.box(7, 4, 5, 7, 5, 5, k.mFence);
        for (int32_t z = 0; z <= 5; ++z) {
            for (int32_t x = 0; x <= 8; ++x)
                b.column(x, z);
        }
    }

    bool touchesChunk(const StructurePiece::Box &box, int32_t chunkX, int32_t chunkZ) {
        return box.mMaxX >= chunkX * 16 && box.mMinX <= chunkX * 16 + 15 && box.mMaxZ >= chunkZ * 16
               && box.mMinZ <= chunkZ * 16 + 15;
    }

    ChunkStructureBounds::Box boundsOf(const StructurePiece::Box &box) {
        ChunkStructureBounds::Box out;
        out.minX = box.mMinX;
        out.minY = box.mMinY;
        out.minZ = box.mMinZ;
        out.maxX = box.mMaxX;
        out.maxY = box.mMaxY;
        out.maxZ = box.mMaxZ;
        return out;
    }
}

const char *const NetherFortressStructure::LOOT_TABLE = "loot_tables/chests/nether_bridge.json";

void NetherFortressStructure::build(const Piece &piece, Level &level, LevelChunk &chunk, std::mt19937 &random) {
    Builder builder(piece, level, chunk);
    switch (piece.mType) {
        case Type::Start:
        case Type::BridgeCrossing:
            bridgeCrossing(builder);
            break;
        case Type::BridgeStraight:
            bridgeStraight(builder);
            break;
        case Type::BridgeEndFiller:
            bridgeEndFiller(builder);
            break;
        case Type::RoomCrossing:
            roomCrossing(builder);
            break;
        case Type::StairsRoom:
            stairsRoom(builder);
            break;
        case Type::MonsterThrone:
            monsterThrone(builder);
            break;
        case Type::CastleEntrance:
            castleEntrance(builder);
            break;
        case Type::CastleStalkRoom:
            castleStalkRoom(builder);
            break;
        case Type::CastleSmallCorridor:
            smallCorridor(builder);
            break;
        case Type::CastleSmallCorridorCrossing:
            corridorCrossing(builder);
            break;
        case Type::CastleSmallCorridorRightTurn:
            corridorTurn(builder, random, true);
            break;
        case Type::CastleSmallCorridorLeftTurn:
            corridorTurn(builder, random, false);
            break;
        case Type::CastleCorridorStairs:
            corridorStairs(builder);
            break;
        case Type::CastleCorridorTBalcony:
            corridorBalcony(builder);
            break;
    }
}

void NetherFortressStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    const std::vector<std::shared_ptr<const Plan>> plans = plansNear(generator, chunk.getX(), chunk.getZ());
    if (plans.empty())
        return;

    // one post process random for every fortress piece of the chunk
    std::mt19937 random = StructureFeature::createPostProcessRandom(generator.getSeed(), chunk.getX(), chunk.getZ());
    const ChunkStructureBounds::Box chunkBox =
            ChunkStructureBounds::chunkBox(chunk.getX(), chunk.getZ(), level.getMinY(), level.getMaxY());
    for (const std::shared_ptr<const Plan> &plan: plans) {
        std::vector<ChunkStructureBounds::Box> pieces;
        for (const Piece &piece: plan->mPieces) {
            pieces.push_back(boundsOf(piece.getBox()));
            if (touchesChunk(piece.getBox(), chunk.getX(), chunk.getZ()))
                build(piece, level, chunk, random);
        }
        chunk.getStructureBounds().addStart(FORTRESS_NAME, 0, boundsOf(plan->mBox), pieces, chunkBox);
    }
}
