#include "World/Level/LevelGen/Structure/MonumentStructure.h"

#include "World/Actor/Mob/Hostile/ElderGuardianActor.h"
#include "World/Actor/Mob/Hostile/GuardianActor.h"
#include "World/Level/Chunk/ChunkStructureBounds.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Overworld/OverworldGeneratorConstants.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"

#include <cmath>
#include <deque>
#include <map>
#include <mutex>
#include <tuple>

using MonumentPieces::Piece;
using MonumentPieces::Type;

namespace {
    const size_t CACHED_PLANS = 64;

    // the building spans 21 blocks before its start chunk and 36 after
    const int32_t REACH = 2;

    const StructureFeature *feature() {
        return StructureFeature::get("monument");
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

    bool inChunk(int32_t x, int32_t z, const LevelChunk &chunk) {
        return (x >> 4) == chunk.getX() && (z >> 4) == chunk.getZ();
    }

    Tag generatedMob(const char *identifier, float x, float y, float z) {
        Tag location = Tag::ofList(Tag::Type::Float);
        location.addToList(Tag::ofFloat(x));
        location.addToList(Tag::ofFloat(y));
        location.addToList(Tag::ofFloat(z));

        Tag mob = Tag::ofCompound();
        mob.putString("identifier", identifier);
        mob.put("Pos", location);
        mob.putByte("Persistent", 1);
        return mob;
    }

    // the elder of a wing or the penthouse, by the chunk holding the middle of its box
    void spawnElder(const Piece &piece, Level &level, const LevelChunk &chunk) {
        const StructurePiece::Box &box = piece.getBox();
        const int32_t middleX = box.mMinX + (box.mMaxX - box.mMinX + 1) / 2;
        const int32_t middleZ = box.mMinZ + (box.mMaxZ - box.mMinZ + 1) / 2;
        if (!inChunk(middleX, middleZ, chunk))
            return;

        int32_t x = 6;
        int32_t z = 6;
        if (piece.mType == Type::Penthouse)
            z = 3;
        else if (piece.mSecondWing)
            x = 16;
        level.addGeneratedEntity(generatedMob(ElderGuardianActor::IDENTIFIER, (float) piece.worldX(x, z) + 0.5f,
                                              (float) piece.worldY(1), (float) piece.worldZ(x, z) + 0.5f));
    }

    // a guardian somewhere in one of the rooms; the last child, the penthouse, is never drawn
    void spawnGuardian(const MonumentPieces::Layout &layout, Level &level, const LevelChunk &chunk,
                       std::mt19937 &random) {
        const int32_t children = (int32_t) layout.mPieces.size() - 1;
        if (children <= 0)
            return;

        const int32_t drawn = children - 1 <= 0 ? 0 : FeatureRandom::nextInt(random, children - 1);
        const StructurePiece::Box &box = layout.mPieces[(size_t) drawn + 1].getBox();
        const float x = (float) (box.mMaxX - box.mMinX) * FeatureRandom::nextFloat(random) + (float) box.mMinX;
        const float y = (float) (box.mMaxY - box.mMinY) * FeatureRandom::nextFloat(random) + (float) box.mMinY;
        const float z = (float) (box.mMaxZ - box.mMinZ) * FeatureRandom::nextFloat(random) + (float) box.mMinZ;
        // stored with the chunk holding it
        if (inChunk((int32_t) std::floor(x), (int32_t) std::floor(z), chunk))
            level.addGeneratedEntity(generatedMob(GuardianActor::IDENTIFIER, x, y, z));
    }
}

const char *const MonumentStructure::NAME = "minecraft:monument";

std::shared_ptr<const MonumentPieces::Layout> MonumentStructure::plan(const ChunkGenerator &generator,
                                                                       int32_t startX, int32_t startZ) {
    // the plan reads only the seed and the biomes: every worker's generator shares it
    using Key = std::tuple<int64_t, int32_t, int32_t>;
    static std::mutex mutex;
    static std::map<Key, std::shared_ptr<const MonumentPieces::Layout>> plans;
    static std::deque<Key> order;

    const Key key = {generator.getSeed(), startX, startZ};
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = plans.find(key);
        if (found != plans.end())
            return found->second;
    }

    // planned unlocked: other workers keep planning; a missing start is cached too, its biome checks are slow
    std::shared_ptr<const MonumentPieces::Layout> planned;
    const StructureFeature *monument = feature();
    if (monument != nullptr && monument->isStartChunk(generator, startX, startZ))
        planned = std::make_shared<const MonumentPieces::Layout>(MonumentPieces::plan(
                generator.getSeed(), startX, startZ, OverworldGeneratorConstants::SEA_LEVEL));

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

void MonumentStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    const StructureFeature *monument = feature();
    if (monument == nullptr || generator.getDimensionType() != monument->getDimension())
        return;

    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();
    const int64_t seed = generator.getSeed();
    std::mt19937 random = StructureFeature::createPostProcessRandom(seed, chunkX, chunkZ);
    const ChunkStructureBounds::Box chunkBox =
            ChunkStructureBounds::chunkBox(chunkX, chunkZ, level.getMinY(), level.getMaxY());
    for (const StructureChunkPos &start:
         monument->getCandidatesIn(seed, chunkX - REACH, chunkZ - REACH, chunkX + REACH, chunkZ + REACH)) {
        const std::shared_ptr<const MonumentPieces::Layout> layout = plan(generator, start.x, start.z);
        if (layout == nullptr)
            continue;

        const StructurePiece::Box &box = layout->mPieces.front().getBox();
        if (box.mMaxX < chunkX * 16 || box.mMinX > chunkX * 16 + 15 || box.mMaxZ < chunkZ * 16
            || box.mMinZ > chunkZ * 16 + 15)
            continue;

        MonumentPieces::postProcess(*layout, level, chunk, random, OverworldGeneratorConstants::SEA_LEVEL);

        // the mob pass draws from its own random: the building's guardian first, then the elders
        std::mt19937 mobs = StructureFeature::createPostProcessRandom(seed, chunkX, chunkZ);
        spawnGuardian(*layout, level, chunk, mobs);
        for (const Piece &piece: layout->mPieces) {
            if (piece.mType == Type::Wing || piece.mType == Type::Penthouse)
                spawnElder(piece, level, chunk);
        }

        // the start holds the building alone: the rooms are its children
        chunk.getStructureBounds().addStart(NAME, 0, boundsOf(box), {boundsOf(box)}, chunkBox);
    }
}
