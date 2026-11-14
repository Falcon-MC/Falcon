#include "World/Level/LevelGen/Structure/NetherFortressStructure.h"

#include "World/Level/LevelGen/ChunkGenerator.h"
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
