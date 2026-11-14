#include "World/Level/LevelGen/Structure/StructurePieceWriter.h"

#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/Actor/MobSpawnerBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Structure/StructureMaterials.h"

namespace {
    // the chest takes its direction as a data value: its old facing_direction, renamed cardinal_direction by the
    // 1.20.40 upgrade schema (2 north, 3 south, 4 west, 5 east)
    const char *chestDirection(int32_t direction) {
        switch (direction) {
            case 3:
                return "south";
            case 4:
                return "west";
            case 5:
                return "east";
            default:
                return "north";
        }
    }
}

StructurePieceWriter::StructurePieceWriter(Level &level, LevelChunk &chunk, const StructurePiece &piece)
        : mLevel(level), mChunk(chunk), mPiece(piece) {}

bool StructurePieceWriter::inChunk(int32_t x, int32_t y, int32_t z) const {
    const int32_t blockY = mPiece.worldY(y);
    return (mPiece.worldX(x, z) >> 4) == mChunk.getX() && (mPiece.worldZ(x, z) >> 4) == mChunk.getZ()
           && blockY >= mLevel.getMinY() && blockY <= mLevel.getMaxY();
}

const BlockState *StructurePieceWriter::getBlock(int32_t x, int32_t y, int32_t z) const {
    if (!inChunk(x, y, z))
        return nullptr;
    return &mChunk.getBlock(mPiece.worldX(x, z) & 15, mPiece.worldY(y), mPiece.worldZ(x, z) & 15);
}

void StructurePieceWriter::placeBlock(const BlockState &state, int32_t x, int32_t y, int32_t z) {
    if (inChunk(x, y, z))
        mChunk.setBlock(mPiece.worldX(x, z) & 15, mPiece.worldY(y), mPiece.worldZ(x, z) & 15, state);
}

void StructurePieceWriter::generateBox(int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY,
                                       int32_t maxZ, const BlockState &edge, const BlockState &inside, bool skipAir) {
    for (int32_t y = minY; y <= maxY; y++) {
        for (int32_t x = minX; x <= maxX; x++) {
            for (int32_t z = minZ; z <= maxZ; z++) {
                if (skipAir) {
                    const BlockState *current = getBlock(x, y, z);
                    if (current == nullptr || DecorationSupport::isAir(*current))
                        continue;
                }

                const bool onEdge = y == minY || y == maxY || x == minX || x == maxX || z == minZ || z == maxZ;
                placeBlock(onEdge ? edge : inside, x, y, z);
            }
        }
    }
}

void StructurePieceWriter::fillColumnDown(const BlockState &state, int32_t x, int32_t y, int32_t z) {
    if (!inChunk(x, y, z))
        return;

    const int32_t localX = mPiece.worldX(x, z) & 15;
    const int32_t localZ = mPiece.worldZ(x, z) & 15;
    int32_t blockY = mPiece.worldY(y);
    while (blockY > mLevel.getMinY() + 1) {
        const BlockState &current = mChunk.getBlock(localX, blockY, localZ);
        if (!DecorationSupport::isAir(current) && !StructureMaterials::isLiquid(current)
            && !StructureMaterials::isPlant(current))
            break;

        mChunk.setBlock(localX, blockY, localZ, state);
        blockY--;
    }
    if (mChunk.getBlock(localX, blockY, localZ).mName == VanillaBlocks::GRASS_BLOCK().getIdentifier())
        mChunk.setBlock(localX, blockY, localZ, VanillaBlocks::DIRT().toBlockState());
}

bool StructurePieceWriter::createChest(std::mt19937 &random, int32_t x, int32_t y, int32_t z, int32_t direction,
                                       const char *lootTable) {
    const BlockState *current = getBlock(x, y, z);
    const BlockState chest = VanillaBlocks::CHEST().toBlockState();
    if (current == nullptr || current->mName == chest.mName)
        return false;

    placeBlock(DecorationSupport::withState(chest, "minecraft:cardinal_direction", chestDirection(direction)), x, y, z);
    mLevel.getBlockActors()
            .getOrCreate<ChestBlockActor>(Vector3i(mPiece.worldX(x, z), mPiece.worldY(y), mPiece.worldZ(x, z)))
            .setLootTable(lootTable, FeatureRandom::nextInt(random));
    return true;
}

bool StructurePieceWriter::createSpawner(int32_t x, int32_t y, int32_t z, const std::string &identifier) {
    if (!inChunk(x, y, z))
        return false;

    placeBlock(VanillaBlocks::MOB_SPAWNER().toBlockState(), x, y, z);
    mLevel.getBlockActors()
            .getOrCreate<MobSpawnerBlockActor>(Vector3i(mPiece.worldX(x, z), mPiece.worldY(y), mPiece.worldZ(x, z)))
            .setEntityIdentifier(identifier);
    return true;
}
