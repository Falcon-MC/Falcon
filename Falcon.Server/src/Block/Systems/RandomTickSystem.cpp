#include "Block/Systems/RandomTickSystem.h"

#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Level/SubChunk.h"

#include <random>
#include <vector>

namespace {
    struct Candidate {
        Vector3i mPosition;
        BlockState mState;
    };

    std::mt19937 &generator() {
        static std::mt19937 random(std::random_device{}());
        return random;
    }

    uint32_t nextUpdateLcg() {
        static uint32_t state = 0x3c6ef35f;
        state = state * 3 + 0x3c6ef35f;
        return state >> 2;
    }
}

int RandomTickSystem::nextInt(int bound) {
    if (bound <= 1)
        return 0;

    std::uniform_int_distribution<int> distribution(0, bound - 1);
    return distribution(generator());
}

int RandomTickSystem::getFullLight(Level &level, const Vector3i &position) {
    if (position.y < level.getMinY() || position.y > level.getMaxY())
        return 0;

    LevelChunk *chunk = level.peekChunkPtr(position.x >> 4, position.z >> 4);
    if (chunk == nullptr)
        return 0;

    const int localX = position.x & 15;
    const int localZ = position.z & 15;

    const int blockLight = chunk->hasBlockLight() ? chunk->getBlockLight(localX, position.y, localZ) : 0;
    if (!chunk->hasSkyLight())
        return blockLight;

    const int skyLight = chunk->getSkyLight(localX, position.y, localZ) - level.getSkyLightSubtracted();
    return skyLight > blockLight ? skyLight : blockLight;
}

int RandomTickSystem::getBlockLight(Level &level, const Vector3i &position) {
    if (position.y < level.getMinY() || position.y > level.getMaxY())
        return 0;

    LevelChunk *chunk = level.peekChunkPtr(position.x >> 4, position.z >> 4);
    if (chunk == nullptr || !chunk->hasBlockLight())
        return 0;

    return chunk->getBlockLight(position.x & 15, position.y, position.z & 15);
}

void RandomTickSystem::tick(ServerNetworkHandler &owner, Level &level) {
    const int32_t speed = level.getGameRules().getInt("randomtickspeed");
    if (speed <= 0)
        return;

    std::vector<Candidate> candidates;

    for (const int64_t column: level.getActiveColumns()) {
        const int32_t chunkX = (int32_t) (column >> 32);
        const int32_t chunkZ = (int32_t) (column & 0xffffffff);

        LevelChunk *chunk = level.peekChunkPtr(chunkX, chunkZ);
        if (chunk == nullptr)
            continue;

        for (int index = 0; index < LevelChunk::SUB_CHUNK_COUNT; index++) {
            const SubChunk &subChunk = chunk->getSubChunk(index);
            if (subChunk.isEmpty())
                continue;

            for (int32_t attempt = 0; attempt < speed; attempt++) {
                const uint32_t lcg = nextUpdateLcg();
                const int localX = (int) (lcg & 0x0f);
                const int localY = (int) ((lcg >> 8) & 0x0f);
                const int localZ = (int) ((lcg >> 16) & 0x0f);

                const BlockState state = subChunk.getBlock(localX, localY, localZ);
                if (state.mName == "minecraft:air")
                    continue;

                Candidate candidate;
                candidate.mPosition = Vector3i((chunkX << 4) + localX,
                                               LevelChunk::MIN_Y + index * 16 + localY,
                                               (chunkZ << 4) + localZ);
                candidate.mState = state;
                candidates.push_back(candidate);
            }
        }
    }

    for (const Candidate &candidate: candidates) {
        const Block *block = VanillaBlocks::fromIdentifier(candidate.mState.mName);
        if (block == nullptr)
            continue;

        const BlockState current = level.getBlockState(candidate.mPosition.x, candidate.mPosition.y,
                                                       candidate.mPosition.z);
        if (current.mName != candidate.mState.mName)
            continue;

        block->onRandomTick(owner, level, candidate.mPosition, current);
    }
}
