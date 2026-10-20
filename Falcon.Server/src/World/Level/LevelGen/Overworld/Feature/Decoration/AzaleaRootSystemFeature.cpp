#include "World/Level/LevelGen/Overworld/Feature/Decoration/AzaleaRootSystemFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Noise/SimplexF.h"
#include "World/Level/LevelGen/Feature/FeatureNoiseHolder.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <algorithm>
#include <memory>
#include <string>

namespace {

    const int32_t REQUIRED_VERTICAL_SPACE_FOR_TREE = 4;
    const int32_t ROOT_COLUMN_MAX_HEIGHT = 96;
    const int32_t MIN_TREE_XZ_DISTANCE = 5;
    const int32_t ROOT_RADIUS = 3;
    const int32_t ROOT_PLACEMENT_ATTEMPTS = 18;
    const int32_t HANGING_ROOT_RADIUS = 5;
    const int32_t HANGING_ROOT_VERTICAL_SPAN = 4;
    const int32_t HANGING_ROOT_PLACEMENT_ATTEMPTS = 32;

    ObjectGeneratorPtr (*gFactory)() = nullptr;

    const BlockState &rootedDirtState() {
        static const BlockState state = VanillaBlocks::DIRT_WITH_ROOTS().toBlockState();
        return state;
    }

    const BlockState &hangingRootsState() {
        static const BlockState state = VanillaBlocks::HANGING_ROOTS().toBlockState();
        return state;
    }

    const SimplexF *ceilingNoise() {
        const FeatureNoiseHolder *holder = FeatureNoiseHolder::get();
        if (holder == nullptr)
            return nullptr;

        return &holder->getMossSnapToCeiling();
    }

    bool isRootReplaceable(const std::string &id) {
        return id == "minecraft:stone"
               || id == "minecraft:deepslate"
               || id == "minecraft:tuff"
               || id == "minecraft:dirt"
               || id == "minecraft:clay"
               || id == "minecraft:moss_block"
               || id == "minecraft:dirt_with_roots";
    }

    bool isTreeBlockId(const std::string &id) {
        if (id.empty())
            return false;

        return id.find("log") != std::string::npos
               || id.find("wood") != std::string::npos
               || id.find("leaves") != std::string::npos
               || id.find("azalea") != std::string::npos;
    }

    bool hasNearbyTree(BlockManager &manager, int32_t centerX, int32_t centerY, int32_t centerZ, int32_t radius) {
        for (int32_t dx = -radius; dx <= radius; dx++) {
            for (int32_t dz = -radius; dz <= radius; dz++) {
                if (dx == 0 && dz == 0)
                    continue;

                for (int32_t dy = -2; dy <= 10; dy++) {
                    if (isTreeBlockId(manager.getBlockAt(centerX + dx, centerY + dy, centerZ + dz).mName))
                        return true;
                }
            }
        }

        return false;
    }

    bool spaceForTree(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
        for (int32_t i = 1; i <= REQUIRED_VERTICAL_SPACE_FOR_TREE; i++) {
            if (manager.getBlockAt(x, y + i, z).mName != "minecraft:air")
                return false;
        }

        return true;
    }

}

void AzaleaRootSystemFeature::setGeneratorFactory(ObjectGeneratorPtr (*factory)()) {
    gFactory = factory;
}

const char *AzaleaRootSystemFeature::name() const {
    return "minecraft:azalea_root_system_snap_to_ceiling_feature";
}

void AzaleaRootSystemFeature::_placeRootedDirtLayer(BlockManager &manager, int32_t originX, int32_t y,
                                                    int32_t originZ) {
    for (int32_t i = 0; i < ROOT_PLACEMENT_ATTEMPTS; i++) {
        const int32_t x = originX + mRandom.nextInt(ROOT_RADIUS) - mRandom.nextInt(ROOT_RADIUS);
        const int32_t z = originZ + mRandom.nextInt(ROOT_RADIUS) - mRandom.nextInt(ROOT_RADIUS);
        if (isRootReplaceable(manager.getBlockAt(x, y, z).mName))
            manager.setBlockStateAt(x, y, z, rootedDirtState());
    }
}

void AzaleaRootSystemFeature::_placeRootedDirtColumnDownward(BlockManager &manager, int32_t originX, int32_t originZ,
                                                             int32_t startY, int32_t endY) {
    for (int32_t y = startY; y >= endY; y--)
        _placeRootedDirtLayer(manager, originX, y, originZ);
}

void AzaleaRootSystemFeature::_placeHangingRoots(BlockManager &manager, int32_t originX, int32_t originY,
                                                 int32_t originZ) {
    for (int32_t i = 0; i < HANGING_ROOT_PLACEMENT_ATTEMPTS; i++) {
        const int32_t x = originX + mRandom.nextInt(HANGING_ROOT_RADIUS) - mRandom.nextInt(HANGING_ROOT_RADIUS);
        const int32_t y = originY + mRandom.nextInt(HANGING_ROOT_VERTICAL_SPAN)
                          - mRandom.nextInt(HANGING_ROOT_VERTICAL_SPAN);
        const int32_t z = originZ + mRandom.nextInt(HANGING_ROOT_RADIUS) - mRandom.nextInt(HANGING_ROOT_RADIUS);

        if (manager.getBlockAt(x, y, z).mName != "minecraft:air")
            continue;

        if (!DecorationSupport::isSolid(manager.getBlockAt(x, y + 1, z)))
            continue;

        manager.setBlockStateAt(x, y, z, hangingRootsState());
    }
}

bool AzaleaRootSystemFeature::_placeDirtAndTree(BlockManager &manager, int32_t originX, int32_t originY,
                                                int32_t originZ, int32_t surfaceY) {
    int32_t chosenTreeY = -1;
    const int32_t maxTreeY = std::min(originY + ROOT_COLUMN_MAX_HEIGHT, surfaceY);

    for (int32_t treeY = maxTreeY; treeY > originY; treeY--) {
        if (manager.getBlockAt(originX, treeY, originZ).mName == "minecraft:air"
            && spaceForTree(manager, originX, treeY, originZ)
            && !hasNearbyTree(manager, originX, treeY, originZ, MIN_TREE_XZ_DISTANCE)) {
            chosenTreeY = treeY;
            break;
        }
    }

    if (chosenTreeY < 0)
        return false;

    const int32_t belowY = chosenTreeY - 1;
    const std::string belowId = manager.getBlockAt(originX, belowY, originZ).mName;
    if (belowId == "minecraft:lava" || belowId == "minecraft:flowing_lava"
        || !DecorationSupport::isSolid(manager.getBlockAt(originX, belowY, originZ)))
        return false;

    if (gFactory == nullptr)
        return false;

    if (gFactory()->generate(manager, mRandom, originX, chosenTreeY, originZ)) {
        _placeRootedDirtColumnDownward(manager, originX, originZ, chosenTreeY - 1, originY);
        return true;
    }

    return false;
}

void AzaleaRootSystemFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunkX, chunkZ) ^ javaStringHash(name()));

    const SimplexF *noisePtr = ceilingNoise();
    if (noisePtr == nullptr)
        return;

    const SimplexF &noise = *noisePtr;
    BlockManager manager(level);

    for (int32_t x = 0; x < 16; x++) {
        const int32_t worldX = (chunkX << 4) + x;
        for (int32_t z = 0; z < 16; z++) {
            const int32_t worldZ = (chunkZ << 4) + z;
            if (noise.noise2D((float) worldX * 0.25f, (float) worldZ * 0.25f, true) <= 0.6f)
                continue;

            const int32_t surfaceY = chunk.getHeight(x, z);
            for (int32_t y = surfaceY; y > LevelChunk::MIN_Y + 8; y--) {
                if ((int32_t) chunk.getBiomeAt(x, y, z) != BiomeIds::LUSH_CAVES)
                    continue;

                if (!DecorationSupport::isAir(chunk.getBlock(x, y, z)))
                    continue;

                if (!DecorationSupport::isSolid(manager.getBlockAt(worldX, y + 1, worldZ)))
                    continue;

                if (_placeDirtAndTree(manager, worldX, y, worldZ, surfaceY)) {
                    _placeHangingRoots(manager, worldX, y, worldZ);
                    break;
                }
            }
        }
    }

    queueObject(manager);
}
