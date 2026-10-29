#include "World/Level/LevelGen/Overworld/Feature/Decoration/MonsterRoomFeature.h"

#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {

    ObjectGeneratorPtr (*gFactory)() = nullptr;

}

void MonsterRoomFeature::setGeneratorFactory(ObjectGeneratorPtr (*factory)()) {
    gFactory = factory;
}

const char *MonsterRoomFeature::name() const {
    return "minecraft:monster_room";
}

void MonsterRoomFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ (DecorationSupport::chunkHash(chunkX, chunkZ) + javaStringHash(name())));

    const int32_t sourceX = chunkX << 4;
    const int32_t sourceZ = chunkZ << 4;

    BlockManager probe(level);

    for (int32_t chance = 0; chance < 8; ++chance) {
        const int32_t x = sourceX + mRandom.nextBoundedInt(16) + 8;
        const int32_t y = mRandom.nextBoundedInt(384) - 64;
        const int32_t z = sourceZ + mRandom.nextBoundedInt(16) + 8;

        const int32_t xv = mRandom.nextBoundedInt(2) + 2;
        const int32_t x1 = -xv - 1;
        const int32_t x2 = xv + 1;

        const int32_t zv = mRandom.nextBoundedInt(2) + 2;
        const int32_t z1 = -zv - 1;
        const int32_t z2 = zv + 1;

        int32_t t = 0;
        bool aborted = false;

        for (int32_t dx = x1; dx <= x2 && !aborted; ++dx) {
            for (int32_t dy = -1; dy <= 4 && !aborted; ++dy) {
                for (int32_t dz = z1; dz <= z2; ++dz) {
                    const int32_t tx = x + dx;
                    const int32_t ty = y + dy;
                    const int32_t tz = z + dz;

                    const bool solid = DecorationSupport::isSolid(probe.getBlockAt(tx, ty, tz));

                    if (dy == -1 && !solid) {
                        aborted = true;
                        break;
                    }

                    if (dy == 4 && !solid) {
                        aborted = true;
                        break;
                    }

                    if ((dx == x1 || dx == x2 || dz == z1 || dz == z2) && dy == 0
                        && DecorationSupport::isAir(probe.getBlockAt(tx, ty + 1, tz)))
                        ++t;
                }
            }
        }

        if (aborted)
            continue;

        if (t >= 1 && t <= 5) {
            BlockManager manager(level);
            if (gFactory != nullptr)
                gFactory()->generate(manager, mRandom, x, y, z);

            queueObject(manager);
        }
    }
}
