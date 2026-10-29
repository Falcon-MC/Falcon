#include "World/Level/LevelGen/Nether/Feature/GlowStoneFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {
    const BlockState &glowstoneState() {
        static const BlockState state = VanillaBlocks::GLOWSTONE().toBlockState();
        return state;
    }

    const int32_t FACE_OFFSETS[6][3] = {
            {0,  -1, 0},
            {0,  1,  0},
            {0,  0,  -1},
            {0,  0,  1},
            {-1, 0,  0},
            {1,  0,  0}
    };
}

const char *GlowStoneFeature::name() const {
    return "nether_glowstone";
}

int32_t GlowStoneFeature::_getHighestWorkableBlock(const LevelChunk &chunk, int32_t x, int32_t z) {
    int32_t y = 125;
    for (; y >= 0; y--) {
        if (isAir(chunk.getBlock(x, y, z)))
            break;
    }

    return y == 0 ? -1 : y;
}

bool GlowStoneFeature::_checkAroundBlock(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    for (const int32_t *offset: FACE_OFFSETS) {
        if (isName(manager.getBlockAt(x + offset[0], y + offset[1], z + offset[2]), "minecraft:glowstone"))
            return true;
    }

    return false;
}

void GlowStoneFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    seedForChunk(level, chunkX, chunkZ);

    BlockManager object(level);

    if (mRandom.nextInt(11) == 0) {
        const int32_t x = randomRange(mRandom, chunkX << 4, (chunkX << 4) + 15);
        const int32_t z = randomRange(mRandom, chunkZ << 4, (chunkZ << 4) + 15);
        const int32_t y = _getHighestWorkableBlock(chunk, x & 15, z & 15);

        if (y != -1 && !isName(object.getBlockAt(x, y, z), "minecraft:netherrack")) {
            int32_t count = randomRange(mRandom, 40, 60);
            object.setBlockStateAt(x, y, z, glowstoneState());

            int32_t cyclesNum = 0;
            while (count != 0) {
                if (cyclesNum == 1500)
                    break;

                const int32_t spawnX = x + mRandom.nextInt(9) - mRandom.nextInt(9);
                const int32_t spawnY = y - mRandom.nextInt(9);
                const int32_t spawnZ = z + mRandom.nextInt(9) - mRandom.nextInt(9);

                if (cyclesNum % 128 == 0 && cyclesNum != 0) {
                    const int32_t offsetX = x + mRandom.nextInt(-3, 3);
                    const int32_t offsetY = y - mRandom.nextInt(5);
                    const int32_t offsetZ = z + mRandom.nextInt(-3, 3);
                    object.setBlockStateAt(offsetX, offsetY, offsetZ, glowstoneState());
                    count--;
                }

                if (_checkAroundBlock(object, spawnX, spawnY, spawnZ)) {
                    object.setBlockStateAt(spawnX, spawnY, spawnZ, glowstoneState());
                    count--;
                }

                cyclesNum++;
            }
        }
    }

    queueObject(object);
}
