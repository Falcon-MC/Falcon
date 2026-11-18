#include "World/Level/LevelGen/Structure/MineshaftStructure.h"

#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Random/GameRandom.h"
#include "World/Level/LevelGen/Structure/StructureConnections.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureMaterials.h"
#include "World/Level/LevelGen/Structure/StructurePieceWriter.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <utility>

namespace {
    using MineshaftStructure::Piece;
    using MineshaftStructure::Type;
    using Box = StructurePiece::Box;

    // the door can be 80 blocks out and the longest corridor extends 19 more
    const int32_t REACH = 7;
    const char *const MINESHAFT_LOOT = "loot_tables/chests/abandoned_mineshaft.json";

    struct Palette {
        BlockState air = VanillaBlocks::AIR().toBlockState();
        BlockState web = VanillaBlocks::WEB().toBlockState();
        BlockState torch = VanillaBlocks::TORCH().toBlockState();
        BlockState rail = VanillaBlocks::RAIL().toBlockState();
        BlockState planks;
        BlockState fence;
        BlockState log;
        BlockState chain = VanillaBlocks::IRON_CHAIN().toBlockState();

        explicit Palette(bool mesa)
                : planks((mesa ? VanillaBlocks::DARK_OAK_PLANKS() : VanillaBlocks::OAK_PLANKS()).toBlockState()),
                  fence((mesa ? VanillaBlocks::DARK_OAK_FENCE() : VanillaBlocks::OAK_FENCE()).toBlockState()),
                  log((mesa ? VanillaBlocks::DARK_OAK_LOG() : VanillaBlocks::OAK_LOG()).toBlockState()) {}

        bool canReplace(const BlockState &block) const {
            return block.mName != planks.mName && block.mName != fence.mName && block.mName != log.mName
                   && block.mName != chain.mName && block.mName != VanillaBlocks::BEDROCK().getIdentifier();
        }
    };

    void carve(StructurePieceWriter &writer, const BlockState &air, const Box &box) {
        writer.generateBox(box.mMinX, box.mMinY, box.mMinZ, box.mMaxX, box.mMaxY, box.mMaxZ, air, air, false);
    }

    bool hasLiquid(const Box &box, const LevelChunk &chunk, const Box &clip) {
        const int32_t minX = std::max(box.mMinX - 1, clip.mMinX);
        const int32_t minY = std::max(box.mMinY - 1, clip.mMinY);
        const int32_t minZ = std::max(box.mMinZ - 1, clip.mMinZ);
        const int32_t maxX = std::min(box.mMaxX + 1, clip.mMaxX);
        const int32_t maxY = std::min(box.mMaxY + 1, clip.mMaxY);
        const int32_t maxZ = std::min(box.mMaxZ + 1, clip.mMaxZ);
        for (int32_t x = minX; x <= maxX; ++x) {
            for (int32_t z = minZ; z <= maxZ; ++z) {
                for (int32_t y = minY; y <= maxY; ++y) {
                    if (x != minX && x != maxX && y != minY && y != maxY && z != minZ && z != maxZ)
                        continue;
                    if (StructureMaterials::isLiquid(chunk.getBlock(x & 15, y, z & 15)))
                        return true;
                }
            }
        }
        return false;
    }

    void room(const Piece &piece, StructurePieceWriter &writer, const Palette &blocks, const Box &clip) {
        const Box &box = piece.getBox();
        carve(writer, blocks.air, {box.mMinX, box.mMinY + 1, box.mMinZ,
                                  box.mMaxX, std::min(box.mMinY + 3, box.mMaxY), box.mMaxZ});
        for (const Box &door: piece.mDoorways)
            carve(writer, blocks.air, {door.mMinX, door.mMaxY - 2, door.mMinZ, door.mMaxX, door.mMaxY, door.mMaxZ});

        const int32_t startY = box.mMinY + 4;
        const float width = (float) (box.mMaxX - box.mMinX + 1);
        const float height = (float) (box.mMaxY - startY + 1);
        const float length = (float) (box.mMaxZ - box.mMinZ + 1);
        const float centerX = (float) box.mMinX + width / 2;
        const float centerZ = (float) box.mMinZ + length / 2;
        for (int32_t y = startY; y <= box.mMaxY; ++y) {
            for (int32_t x = std::max(clip.mMinX, box.mMinX); x <= std::min(clip.mMaxX, box.mMaxX); ++x) {
                for (int32_t z = std::max(clip.mMinZ, box.mMinZ); z <= std::min(clip.mMaxZ, box.mMaxZ); ++z) {
                    const float dx = ((float) x - centerX) / (width / 2);
                    const float dy = (float) (y - startY) / height;
                    const float dz = ((float) z - centerZ) / (length / 2);
                    if (dx * dx + dy * dy + dz * dz <= 1.05f)
                        writer.placeBlock(blocks.air, x, y, z);
                }
            }
        }
    }

    void crossing(const Piece &piece, Level &level, LevelChunk &chunk, const Palette &blocks) {
        const Box &box = piece.getBox();
        const StructurePiece world(box, StructurePiece::NO_ORIENTATION);
        StructurePieceWriter writer(level, chunk, world,
                                    [&](const BlockState &state) { return blocks.canReplace(state); });
        const auto clearCross = [&](int32_t minY, int32_t maxY) {
            carve(writer, blocks.air, {box.mMinX + 1, minY, box.mMinZ, box.mMaxX - 1, maxY, box.mMaxZ});
            carve(writer, blocks.air, {box.mMinX, minY, box.mMinZ + 1, box.mMaxX, maxY, box.mMaxZ - 1});
        };
        if (box.mMaxY - box.mMinY > 2) {
            clearCross(box.mMinY, box.mMinY + 2);
            clearCross(box.mMaxY - 2, box.mMaxY);
            carve(writer, blocks.air, {box.mMinX + 1, box.mMinY + 3, box.mMinZ + 1,
                                       box.mMaxX - 1, box.mMinY + 3, box.mMaxZ - 1});
        } else {
            clearCross(box.mMinY, box.mMaxY);
        }

        for (int32_t x: {box.mMinX + 1, box.mMaxX - 1}) {
            for (int32_t z: {box.mMinZ + 1, box.mMaxZ - 1}) {
                const BlockState *ceiling = writer.getBlock(x, box.mMaxY + 1, z);
                if (ceiling && !DecorationSupport::isAir(*ceiling))
                    writer.generateBox(x, box.mMinY, z, x, box.mMaxY, z, blocks.planks, blocks.air, false);
            }
        }
        for (int32_t x = box.mMinX; x <= box.mMaxX; ++x) {
            for (int32_t z = box.mMinZ; z <= box.mMaxZ; ++z) {
                const int32_t y = box.mMinY - 1;
                const BlockState *floor = writer.getBlock(x, y, z);
                if (floor && y + 1 < chunk.getHeight(x & 15, z & 15)
                    && (DecorationSupport::isAir(*floor) || floor->mName == blocks.chain.mName))
                    chunk.setBlock(x & 15, y, z & 15, blocks.planks);
            }
        }
    }

    void stairs(StructurePieceWriter &writer, const Palette &blocks) {
        carve(writer, blocks.air, {0, 5, 0, 2, 7, 1});
        carve(writer, blocks.air, {0, 0, 7, 2, 2, 8});
        for (int32_t z = 2; z <= 6; ++z)
            carve(writer, blocks.air, {0, std::max(1, 6 - z), z, 2, 9 - z, z});
    }

    bool isSupportGap(const BlockState &block) {
        return DecorationSupport::isAir(block) || LiquidView(block).isWater()
               || block.mName == VanillaBlocks::GLOW_LICHEN().getIdentifier()
               || block.mName == VanillaBlocks::SEAGRASS().getIdentifier()
               || block.mName == VanillaBlocks::SHORT_GRASS().getIdentifier()
               || block.mName == VanillaBlocks::FIRE().getIdentifier();
    }

    void supportColumn(StructurePieceWriter &writer, const Palette &blocks, int32_t x, int32_t z) {
        bool descend = true;
        bool ascend = true;
        for (int32_t distance = 1; descend || ascend; ++distance) {
            if (descend) {
                const BlockState *block = writer.getBlock(x, -1 - distance, z);
                descend = distance < 10 && block != nullptr;
                if (block && (LiquidView(*block).isLava() || !isSupportGap(*block))) {
                    if (!LiquidView(*block).isLava() && block->mName != blocks.rail.mName) {
                        for (int32_t y = -distance; y < -1; ++y)
                            writer.placeBlock(blocks.log, x, y, z);
                        return;
                    }
                    descend = false;
                }
            }
            if (ascend) {
                const BlockState *block = writer.getBlock(x, distance - 1, z);
                ascend = distance < 21 && block != nullptr;
                if (block && !isSupportGap(*block) && BlockSupport::isSolid(*block)
                    && !DecorationSupport::isTransparent(*block)) {
                    writer.placeBlock(blocks.fence, x, 0, z);
                    for (int32_t y = 1; y < distance - 1; ++y)
                        writer.placeBlock(blocks.chain, x, y, z);
                    return;
                }
            }
        }
    }

    void corridor(const Piece &piece, StructurePieceWriter &writer, const Palette &blocks, Level &level,
                  LevelChunk &chunk, std::mt19937 &random, std::vector<Vector3i> &fences) {
        const Box &box = piece.getBox();
        const int32_t direction = piece.getOrientation();
        const int32_t length = (direction & 1) == 0 ? box.mMaxZ - box.mMinZ + 1 : box.mMaxX - box.mMinX + 1;
        const auto belowSurface = [&](int32_t x, int32_t y, int32_t z) {
            const int32_t worldX = piece.worldX(x, z);
            const int32_t worldZ = piece.worldZ(x, z);
            const LevelChunk *column = level.peekChunkPtr(worldX >> 4, worldZ >> 4);
            return column && piece.worldY(y) < column->getHeight(worldX & 15, worldZ & 15);
        };
        const auto isAir = [&](int32_t x, int32_t y, int32_t z) {
            const BlockState *state = writer.getBlock(x, y, z);
            return state == nullptr || DecorationSupport::isAir(*state);
        };

        const auto hasCeiling = [&](int32_t x, int32_t z) {
            const BlockState *state = level.peekBlockPtr(piece.worldX(x, z), piece.worldY(3), piece.worldZ(x, z));
            return state && !DecorationSupport::isAir(*state);
        };

        carve(writer, blocks.air, {0, 0, 0, 2, 1, length - 1});
        for (int32_t x = 0; x <= 2; ++x) {
            for (int32_t z = 0; z < length; ++z) {
                if (GameRandom::nextFloat(random) <= 0.8f)
                    writer.placeBlock(blocks.air, x, 2, z);
            }
        }
        if (piece.mSpiders) {
            for (int32_t y = 0; y <= 1; ++y) {
                for (int32_t x = 0; x <= 2; ++x) {
                    for (int32_t z = 0; z < length; ++z) {
                        if (GameRandom::nextFloat(random) <= 0.6f && belowSurface(x, y + 1, z))
                            writer.placeBlock(blocks.web, x, y, z);
                    }
                }
            }
        }

        const auto noteFence = [&](int32_t x, int32_t y, int32_t z) {
            if (writer.inChunk(x, y, z))
                fences.emplace_back(piece.worldX(x, z), piece.worldY(y), piece.worldZ(x, z));
        };

        constexpr std::array<const char *, 4> FORWARD = {"south", "west", "north", "east"};
        StructurePieceWriter raw(level, chunk, piece);
        bool spiderPlaced = false;
        for (int32_t section = 0; section < length; section += 5) {
            const int32_t z = section + 2;
            if (hasCeiling(0, z) && hasCeiling(1, z) && hasCeiling(2, z)) {
                for (int32_t x: {0, 2}) {
                    for (int32_t y = 0; y <= 1; ++y) {
                        writer.placeBlock(blocks.fence, x, y, z);
                        noteFence(x, y, z);
                    }
                }
                if (GameRandom::nextInt(random, 4) == 0) {
                    writer.placeBlock(blocks.planks, 0, 2, z);
                    writer.placeBlock(blocks.planks, 2, 2, z);
                } else {
                    writer.generateBox(0, 2, z, 2, 2, z, blocks.planks, blocks.air, false);
                    if (GameRandom::nextFloat(random) < 0.05f)
                        writer.placeBlock(DecorationSupport::withState(blocks.torch, "torch_facing_direction",
                                                                       FORWARD[(size_t) ((direction + 2) & 3)]),
                                          1, 2, z - 1);
                    if (GameRandom::nextFloat(random) < 0.05f)
                        writer.placeBlock(DecorationSupport::withState(blocks.torch, "torch_facing_direction",
                                                                       FORWARD[(size_t) direction]), 1, 2, z + 1);
                }
            }

            const auto tryWeb = [&](int32_t x, int32_t queryZ, float chance) {
                if (!belowSurface(x, 3, queryZ) || GameRandom::nextFloat(random) >= chance)
                    return;
                const int32_t webZ = queryZ - 1;
                const int neighbors = !isAir(x, 1, webZ) + !isAir(x, 3, webZ)
                                      + !isAir(x - 1, 2, webZ) + !isAir(x + 1, 2, webZ)
                                      + !isAir(x, 2, webZ - 1) + !isAir(x, 2, webZ + 1);
                if (neighbors >= 2)
                    writer.placeBlock(blocks.web, x, 2, webZ);
            };
            tryWeb(0, z, 0.1f);
            tryWeb(2, z - 1, 0.1f);
            for (int32_t x: {0, 2})
                tryWeb(x, z + 1, 0.1f);
            for (int32_t x: {0, 2})
                tryWeb(x, z - 2, 0.05f);
            for (int32_t x: {0, 2})
                tryWeb(x, z + 2, 0.05f);

            if (piece.mSpiders && !spiderPlaced) {
                const int32_t spiderZ = z - 1 + GameRandom::nextInt(random, 3);
                if (writer.inChunk(1, 0, spiderZ) && belowSurface(1, 0, spiderZ))
                    spiderPlaced = raw.createSpawner(1, 0, spiderZ, "minecraft:cave_spider");
            }
        }

        for (int32_t x = 0; x <= 2; ++x) {
            for (int32_t z = 0; z < length; ++z) {
                const BlockState *floor = writer.getBlock(x, -1, z);
                if (floor && belowSurface(x, 0, z)
                    && (DecorationSupport::isAir(*floor) || floor->mName == blocks.chain.mName))
                    chunk.setBlock(piece.worldX(x, z) & 15, piece.worldY(-1), piece.worldZ(x, z) & 15, blocks.planks);
            }
        }

        for (int32_t x: {0, 2}) {
            for (int32_t z: {2, length == 5 ? -1 : length - 3}) {
                if (z < 0)
                    continue;
                const BlockState *floor = writer.getBlock(x, -1, z);
                if (floor && floor->mName == blocks.planks.mName) {
                    supportColumn(raw, blocks, x, z);
                    noteFence(x, 0, z);
                }
            }
        }

        if (!piece.mRails)
            return;
        const BlockState rail = DecorationSupport::withState(blocks.rail, "rail_direction", (direction & 1));
        for (int32_t z = 0; z < length; ++z) {
            const BlockState *floor = writer.getBlock(1, -1, z);
            if (floor && BlockSupport::isSolid(*floor) &&
                GameRandom::nextFloat(random) < (belowSurface(1, 0, z) ? 0.7f : 0.9f))
                writer.placeBlock(rail, 1, 0, z);
        }
    }

    void placeMinecart(const Piece &piece, StructurePieceWriter &writer, Level &level, const Palette &blocks,
                       std::mt19937 &random, int32_t x, int32_t z) {
        if (GameRandom::nextInt(random, 100) != 0)
            return;

        const BlockState *at = writer.getBlock(x, 0, z);
        const BlockState *below = writer.getBlock(x, -1, z);
        if (!at || !below || !DecorationSupport::isAir(*at) || DecorationSupport::isAir(*below))
            return;

        const bool across = (GameRandom::nextUnsigned(random) & 0x8000000U) != 0;
        const int32_t direction = (piece.getOrientation() & 1) ^ (across ? 1 : 0);
        writer.placeBlock(DecorationSupport::withState(blocks.rail, "rail_direction", direction), x, 0, z);

        Tag position = Tag::ofList(Tag::Type::Float);
        position.addToList(Tag::ofFloat((float) piece.worldX(x, z)));
        position.addToList(Tag::ofFloat((float) piece.worldY(0)));
        position.addToList(Tag::ofFloat((float) piece.worldZ(x, z)));

        Tag cart = Tag::ofCompound();
        cart.putString("identifier", "minecraft:chest_minecart");
        cart.put("Pos", position);
        cart.putString("LootTable", MINESHAFT_LOOT);
        cart.putInt("LootTableSeed", GameRandom::nextInt(random));
        level.addGeneratedEntity(std::move(cart));
    }

}

void MineshaftStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();
    const ChunkStructureBounds::Box chunkBox = ChunkStructureBounds::chunkBox(chunkX, chunkZ, level.getMinY(),
                                                                              level.getMaxY());
    const Box clip{chunkBox.minX, chunkBox.minY, chunkBox.minZ, chunkBox.maxX, chunkBox.maxY, chunkBox.maxZ};
    auto random = StructureFeature::createPostProcessRandom(generator.getSeed(), chunkX, chunkZ);
    auto lootRandom = random;
    std::vector<Vector3i> fences;
    for (int32_t x = chunkX - REACH; x <= chunkX + REACH; ++x) {
        for (int32_t z = chunkZ - REACH; z <= chunkZ + REACH; ++z) {
            const auto layout = plan(generator, x, z);
            if (!layout || !layout->mBounds.intersects(clip))
                continue;

            const Palette blocks(layout->mMesa);
            std::vector<ChunkStructureBounds::Box> boxes;
            std::vector<const Piece *> corridors;
            for (const Piece &piece: layout->mPieces) {
                const Box &box = piece.getBox();
                if (!box.intersects(clip))
                    continue;
                const int32_t centerX = (std::max(box.mMinX - 1, clip.mMinX)
                                         + std::min(box.mMaxX + 1, clip.mMaxX)) / 2;
                const int32_t centerY = (std::max(box.mMinY - 1, clip.mMinY)
                                         + std::min(box.mMaxY + 1, clip.mMaxY)) / 2;
                const int32_t centerZ = (std::max(box.mMinZ - 1, clip.mMinZ)
                                         + std::min(box.mMaxZ + 1, clip.mMaxZ)) / 2;
                if (chunk.getBiomeAt(centerX & 15, centerY, centerZ & 15) == BiomeIds::DEEP_DARK
                    || hasLiquid(box, chunk, clip))
                    continue;
                boxes.push_back({box.mMinX, box.mMinY, box.mMinZ, box.mMaxX, box.mMaxY, box.mMaxZ});

                StructurePieceWriter writer(level, chunk, piece,
                                            [&](const BlockState &state) { return blocks.canReplace(state); });
                switch (piece.mType) {
                    case Type::Room: room(piece, writer, blocks, clip); break;
                    case Type::Crossing: crossing(piece, level, chunk, blocks); break;
                    case Type::Stairs: stairs(writer, blocks); break;
                    case Type::Corridor:
                        corridor(piece, writer, blocks, level, chunk, random, fences);
                        corridors.push_back(&piece);
                        break;
                }
            }
            for (const Piece *piece: corridors) {
                StructurePieceWriter writer(level, chunk, *piece);
                const Box &box = piece->getBox();
                const int32_t length = (piece->getOrientation() & 1) == 0 ? box.mMaxZ - box.mMinZ + 1
                                                                          : box.mMaxX - box.mMinX + 1;
                for (int32_t section = 0; section < length; section += 5) {
                    placeMinecart(*piece, writer, level, blocks, lootRandom, 2, section + 1);
                    placeMinecart(*piece, writer, level, blocks, lootRandom, 0, section + 3);
                }
            }

            const Box &bounds = layout->mBounds;
            chunk.getStructureBounds().addStart("minecraft:mineshaft", 0,
                    {bounds.mMinX, bounds.mMinY, bounds.mMinZ, bounds.mMaxX, bounds.mMaxY, bounds.mMaxZ}, boxes, chunkBox);
        }
    }
    if (fences.empty())
        return;

    const BlockState &air = DecorationSupport::airState();
    const StructureConnections::Lookup blockAt = [&](int32_t x, int32_t y, int32_t z) {
        const BlockState *block = level.peekBlockPtr(x, y, z);
        return block ? *block : air;
    };
    for (const Vector3i &pos: fences) {
        const BlockState &state = chunk.getBlock(pos.x & 15, pos.y, pos.z & 15);
        if (state.mName == VanillaBlocks::OAK_FENCE().getIdentifier()
            || state.mName == VanillaBlocks::DARK_OAK_FENCE().getIdentifier())
            chunk.setBlock(pos.x & 15, pos.y, pos.z & 15, StructureConnections::shaped(state, pos.x, pos.y, pos.z, blockAt));
    }
}
