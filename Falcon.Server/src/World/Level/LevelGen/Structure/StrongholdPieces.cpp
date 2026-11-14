#include "World/Level/LevelGen/Structure/StrongholdStructure.h"

#include "Core/NBT/NbtIo.h"
#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/ChunkStructureBounds.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Random/GameRandom.h"
#include "World/Level/LevelGen/Structure/StrongholdFeature.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "World/Level/LevelGen/Structure/StructureMaterials.h"

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    using Type = StrongholdStructure::Type;
    using Door = StrongholdStructure::Door;
    using Piece = StrongholdStructure::Piece;
    using Box = StructurePiece::Box;

    const char *const CORRIDOR_LOOT = "loot_tables/chests/stronghold_corridor.json";
    const char *const CROSSING_LOOT = "loot_tables/chests/stronghold_crossing.json";
    const char *const LIBRARY_LOOT = "loot_tables/chests/stronghold_library.json";
    const char *const SPAWNER_MOB = "minecraft:silverfish";
    const char *const NAME = "minecraft:stronghold";
    // a start is built for every stronghold start this many chunks around the chunk
    const int32_t REACH = 8;
    // a liquid the pieces lay waits this many ticks for its first update
    const int32_t LIQUID_DELAY = 1;

    // the values of the string states, in the order the game numbers them
    const char *const CARDINALS[4] = {"south", "west", "north", "east"};
    const char *const TORCH_FACINGS[6] = {"unknown", "west", "east", "north", "south", "top"};

    // the data a facing_direction takes when the piece is turned, from north 2 to east 5; north keeps it
    const int32_t FACING_TURNS[4][4] = {{3, 2, 5, 4}, {4, 5, 3, 2}, {2, 3, 4, 5}, {5, 4, 2, 3}};
    // the facing of a torch hung on a block to its west, east, north or south in the piece, by orientation
    const int32_t WEST_WALL_TORCH[4] = {1, 3, 1, 3};
    const int32_t EAST_WALL_TORCH[4] = {2, 4, 2, 4};
    const int32_t NORTH_WALL_TORCH[4] = {3, 2, 4, 1};
    const int32_t SOUTH_WALL_TORCH[4] = {4, 1, 3, 2};
    // the portal frames along the near and far rows and the west and east columns, by orientation
    const int32_t FAR_ROW_FRAMES[4] = {2, 3, 0, 1};
    const int32_t WEST_COLUMN_FRAMES[4] = {3, 0, 3, 0};
    const int32_t EAST_COLUMN_FRAMES[4] = {1, 2, 1, 2};

    const float FRAME_EYE_CHANCE = 0.9f;
    const float WEB_CHANCE = 0.07f;
    const float TORCH_CHANCE = 0.1f;

    BlockState defaultState(const char *name) {
        const Block *block = VanillaBlocks::fromIdentifier(name);
        if (block == nullptr)
            throw std::runtime_error(std::string("Missing block ") + name);
        return block->toBlockState();
    }

    struct Blocks {
        BlockState air = defaultState("minecraft:air");
        BlockState stoneBricks = defaultState("minecraft:stone_bricks");
        BlockState mossy = defaultState("minecraft:mossy_stone_bricks");
        BlockState cracked = defaultState("minecraft:cracked_stone_bricks");
        BlockState infested = defaultState("minecraft:infested_stone_bricks");
        BlockState brickSlab = defaultState("minecraft:stone_brick_slab");
        BlockState smoothSlab = defaultState("minecraft:smooth_stone_slab");
        BlockState smoothDoubleSlab = defaultState("minecraft:smooth_stone_double_slab");
        BlockState ironBars = defaultState("minecraft:iron_bars");
        BlockState torch = defaultState("minecraft:torch");
        BlockState woodenDoor = defaultState("minecraft:wooden_door");
        BlockState ironDoor = defaultState("minecraft:iron_door");
        BlockState stoneButton = defaultState("minecraft:stone_button");
        BlockState ladder = defaultState("minecraft:ladder");
        BlockState stoneStairs = defaultState("minecraft:stone_stairs");
        BlockState brickStairs = defaultState("minecraft:stone_brick_stairs");
        BlockState cobblestone = defaultState("minecraft:cobblestone");
        BlockState planks = defaultState("minecraft:oak_planks");
        BlockState fence = defaultState("minecraft:oak_fence");
        BlockState bookshelf = defaultState("minecraft:bookshelf");
        BlockState web = defaultState("minecraft:web");
        BlockState water = defaultState("minecraft:water");
        BlockState lava = defaultState("minecraft:lava");
        BlockState frame = defaultState("minecraft:end_portal_frame");
        BlockState endPortal = defaultState("minecraft:end_portal");
        BlockState spawner = defaultState("minecraft:mob_spawner");
        BlockState chest = defaultState("minecraft:chest");
    };

    const Blocks &blocks() {
        static const Blocks table;
        return table;
    }

    // a chest takes its old facing data as is: the game never turns it with the piece
    const char *chestDirection(int32_t data) {
        switch (data) {
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

    // the data of a door (block property 0x200) turned with the piece
    int32_t doorData(int32_t orientation, int32_t data) {
        switch (orientation) {
            case StructurePiece::EAST:
                return (data - 1) & 3;
            case StructurePiece::WEST:
                return (data + 1) & 3;
            case StructurePiece::SOUTH: {
                static const int32_t SOUTH_TURN[4] = {2, 3, 0, 1};
                return data >= 0 && data < 4 ? SOUTH_TURN[data] : data;
            }
            default:
                return data;
        }
    }

    int32_t facingData(int32_t orientation, int32_t data) {
        if (data < 2 || data > 5 || orientation < 0 || orientation > 3)
            return data;
        return FACING_TURNS[orientation][data - 2];
    }

    BlockState torchFacing(int32_t value) {
        return DecorationSupport::withState(blocks().torch, "torch_facing_direction", TORCH_FACINGS[value]);
    }

    // the chunk keeps the liquid's first tick in its PendingTicks record, read when the level loads the chunk
    void addLiquidTick(LevelChunk &chunk, int32_t x, int32_t y, int32_t z, const BlockState &state) {
        Tag record = Tag::ofCompound();
        std::vector<Tag> ticks;
        if (!chunk.getPendingTicks().empty()) {
            ReadOnlyBinaryStream stream(chunk.getPendingTicks());
            record = NbtIo::readTag(stream, NbtVariant::LittleEndian);
            const Tag *list = record.get("tickList");
            if (list != nullptr && list->getType() == Tag::Type::List)
                ticks = list->getList();
        } else {
            record.putInt("currentTick", 0);
        }

        Tag tick = Tag::ofCompound();
        tick.putInt("x", x);
        tick.putInt("y", y);
        tick.putInt("z", z);
        tick.put("blockState", state.toNbt());
        tick.putLong("time", (int64_t) record.getInt("currentTick") + LIQUID_DELAY);
        ticks.push_back(tick);
        record.put("tickList", Tag::ofList(Tag::Type::Compound, ticks));

        BinaryStream out;
        NbtIo::writeTag(out, record, NbtVariant::LittleEndian);
        chunk.setPendingTicks(out.getBuffer());
    }

    class Writer {
    public:
        Writer(Level &level, LevelChunk &chunk, const Piece &piece, std::mt19937 &random)
                : mLevel(level), mChunk(chunk), mPiece(piece), mRandom(random), mBlocks(blocks()) {}

        int32_t orientation() const {
            return mPiece.getOrientation();
        }

        bool inChunk(int32_t x, int32_t y, int32_t z) const {
            const int32_t worldY = mPiece.worldY(y);
            return (mPiece.worldX(x, z) >> 4) == mChunk.getX() && (mPiece.worldZ(x, z) >> 4) == mChunk.getZ()
                   && worldY >= LevelChunk::MIN_Y && worldY <= LevelChunk::MAX_Y;
        }

        void place(const BlockState &state, int32_t x, int32_t y, int32_t z) {
            if (!inChunk(x, y, z))
                return;
            const int32_t worldX = mPiece.worldX(x, z);
            const int32_t worldY = mPiece.worldY(y);
            const int32_t worldZ = mPiece.worldZ(x, z);
            mChunk.setBlock(worldX & 15, worldY, worldZ & 15, state);
            if (StructureMaterials::isLiquid(state))
                addLiquidTick(mChunk, worldX, worldY, worldZ, state);
        }

        void box(int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY, int32_t maxZ,
                 const BlockState &edge, const BlockState &inside) {
            for (int32_t y = minY; y <= maxY; ++y) {
                for (int32_t x = minX; x <= maxX; ++x) {
                    for (int32_t z = minZ; z <= maxZ; ++z) {
                        const bool onEdge =
                                y == minY || y == maxY || x == minX || x == maxX || z == minZ || z == maxZ;
                        place(onEdge ? edge : inside, x, y, z);
                    }
                }
            }
        }

        // the walls draw one brick kind each, even outside the chunk; the inside is air and draws nothing
        void stoneBox(int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY, int32_t maxZ) {
            for (int32_t y = minY; y <= maxY; ++y) {
                for (int32_t x = minX; x <= maxX; ++x) {
                    for (int32_t z = minZ; z <= maxZ; ++z) {
                        const bool onEdge =
                                y == minY || y == maxY || x == minX || x == maxX || z == minZ || z == maxZ;
                        place(onEdge ? brick() : mBlocks.air, x, y, z);
                    }
                }
            }
        }

        // a draw for every block of the box, the block placed when it is at most the chance
        void maybeBox(float chance, int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY,
                      int32_t maxZ, const BlockState &state) {
            for (int32_t y = minY; y <= maxY; ++y) {
                for (int32_t x = minX; x <= maxX; ++x) {
                    for (int32_t z = minZ; z <= maxZ; ++z) {
                        if (GameRandom::nextFloat(mRandom) <= chance)
                            place(state, x, y, z);
                    }
                }
            }
        }

        bool chance(float limit) {
            return GameRandom::nextFloat(mRandom) < limit;
        }

        void door(Door type, int32_t x, int32_t y, int32_t z) {
            switch (type) {
                case Door::WoodDoor:
                    frame(x, y, z);
                    doorHalves(mBlocks.woodenDoor, x, y, z);
                    break;
                case Door::Grates:
                    place(mBlocks.air, x + 1, y, z);
                    place(mBlocks.air, x + 1, y + 1, z);
                    place(mBlocks.ironBars, x, y, z);
                    place(mBlocks.ironBars, x, y + 1, z);
                    place(mBlocks.ironBars, x, y + 2, z);
                    place(mBlocks.ironBars, x + 1, y + 2, z);
                    place(mBlocks.ironBars, x + 2, y + 2, z);
                    place(mBlocks.ironBars, x + 2, y + 1, z);
                    place(mBlocks.ironBars, x + 2, y, z);
                    break;
                case Door::IronDoor:
                    frame(x, y, z);
                    doorHalves(mBlocks.ironDoor, x, y, z);
                    place(button(2), x + 2, y + 1, z + 1);
                    place(button(3), x + 2, y + 1, z - 1);
                    break;
                case Door::Opening:
                    box(x, y, z, x + 2, y + 2, z, mBlocks.air, mBlocks.air);
                    break;
            }
        }

        // a chest in the chunk where none stands yet; its loot seed is drawn once it is placed
        void chest(int32_t x, int32_t y, int32_t z, int32_t data, const char *lootTable) {
            if (!inChunk(x, y, z))
                return;
            const int32_t worldX = mPiece.worldX(x, z);
            const int32_t worldY = mPiece.worldY(y);
            const int32_t worldZ = mPiece.worldZ(x, z);
            if (mChunk.getBlock(worldX & 15, worldY, worldZ & 15).mName == mBlocks.chest.mName)
                return;

            place(DecorationSupport::withState(mBlocks.chest, "minecraft:cardinal_direction", chestDirection(data)), x,
                  y, z);
            mLevel.getBlockActors()
                    .getOrCreate<ChestBlockActor>(Vector3i(worldX, worldY, worldZ))
                    .setLootTable(lootTable, GameRandom::nextInt(mRandom));
        }

        // the spawner's save as the game writes a new one set to its mob; the spawner logic reads it on load
        void spawner(int32_t x, int32_t y, int32_t z) {
            if (!inChunk(x, y, z))
                return;
            place(mBlocks.spawner, x, y, z);

            Tag data = Tag::ofCompound();
            data.putString("id", "MobSpawner");
            data.putInt("x", mPiece.worldX(x, z));
            data.putInt("y", mPiece.worldY(y));
            data.putInt("z", mPiece.worldZ(x, z));
            data.putString("EntityIdentifier", SPAWNER_MOB);
            data.putShort("Delay", 20);
            data.putShort("MinSpawnDelay", 200);
            data.putShort("MaxSpawnDelay", 800);
            data.putShort("SpawnCount", 4);
            data.putShort("MaxNearbyEntities", 6);
            data.putShort("RequiredPlayerRange", 16);
            data.putShort("SpawnRange", 4);
            data.putFloat("DisplayEntityWidth", 0.8f);
            data.putFloat("DisplayEntityHeight", 1.8f);
            data.putFloat("DisplayEntityScale", 1.0f);
            mLevel.getBlockActors().keepGenerated(data);
        }

        BlockState stairs(const BlockState &base, int32_t direction) const {
            return DecorationSupport::withState(base, "weirdo_direction", mPiece.stairsDirection(direction));
        }

        BlockState ladder(int32_t data) const {
            return DecorationSupport::withState(mBlocks.ladder, "facing_direction", facingData(orientation(), data));
        }

    private:
        // the stone selector: cracked under 0.2, mossy under 0.5, infested under 0.55, plain otherwise
        const BlockState &brick() {
            const float value = GameRandom::nextFloat(mRandom);
            if (value < 0.2f)
                return mBlocks.cracked;
            if (value < 0.5f)
                return mBlocks.mossy;
            if (value < 0.55f)
                return mBlocks.infested;
            return mBlocks.stoneBricks;
        }

        void frame(int32_t x, int32_t y, int32_t z) {
            place(mBlocks.stoneBricks, x, y, z);
            place(mBlocks.stoneBricks, x, y + 1, z);
            place(mBlocks.stoneBricks, x, y + 2, z);
            place(mBlocks.stoneBricks, x + 1, y + 2, z);
            place(mBlocks.stoneBricks, x + 2, y + 2, z);
            place(mBlocks.stoneBricks, x + 2, y + 1, z);
            place(mBlocks.stoneBricks, x + 2, y, z);
        }

        void doorHalves(const BlockState &base, int32_t x, int32_t y, int32_t z) {
            const BlockState lower = DecorationSupport::withState(base, "minecraft:cardinal_direction",
                                                                  CARDINALS[doorData(orientation(), 3)]);
            place(lower, x + 1, y, z);
            place(DecorationSupport::withState(lower, "upper_block_bit", 1), x + 1, y + 1, z);
        }

        BlockState button(int32_t data) const {
            return DecorationSupport::withState(mBlocks.stoneButton, "facing_direction",
                                                facingData(orientation(), data));
        }

        Level &mLevel;
        LevelChunk &mChunk;
        const Piece &mPiece;
        std::mt19937 &mRandom;
        const Blocks &mBlocks;
    };

    void stairsDown(Writer &writer, const Piece &piece) {
        const Blocks &b = blocks();
        writer.stoneBox(0, 0, 0, 4, 10, 4);
        writer.door(piece.mEntryDoor, 1, 7, 0);
        writer.door(Door::Opening, 1, 1, 4);
        writer.place(b.stoneBricks, 2, 6, 1);
        writer.place(b.stoneBricks, 1, 5, 1);
        writer.place(b.brickSlab, 1, 6, 1);
        writer.place(b.stoneBricks, 1, 5, 2);
        writer.place(b.stoneBricks, 1, 4, 3);
        writer.place(b.brickSlab, 1, 5, 3);
        writer.place(b.stoneBricks, 2, 4, 3);
        writer.place(b.stoneBricks, 3, 3, 3);
        writer.place(b.brickSlab, 3, 4, 3);
        writer.place(b.stoneBricks, 3, 3, 2);
        writer.place(b.stoneBricks, 3, 2, 1);
        writer.place(b.brickSlab, 3, 3, 1);
        writer.place(b.stoneBricks, 2, 2, 1);
        writer.place(b.stoneBricks, 1, 1, 1);
        writer.place(b.brickSlab, 1, 2, 1);
        writer.place(b.stoneBricks, 1, 1, 2);
        writer.place(b.brickSlab, 1, 1, 3);
    }

    void chestCorridor(Writer &writer, const Piece &piece) {
        const Blocks &b = blocks();
        writer.stoneBox(0, 0, 0, 4, 4, 6);
        writer.door(piece.mEntryDoor, 1, 1, 0);
        writer.door(Door::Opening, 1, 1, 6);
        writer.box(3, 1, 2, 3, 1, 4, b.stoneBricks, b.air);
        writer.place(b.brickSlab, 3, 1, 1);
        writer.place(b.brickSlab, 3, 1, 5);
        writer.place(b.brickSlab, 3, 2, 2);
        writer.place(b.brickSlab, 3, 2, 4);
        for (int32_t z = 2; z <= 4; ++z)
            writer.place(b.brickSlab, 2, 1, z);
        writer.chest(3, 2, 3, 4, CORRIDOR_LOOT);
    }

    void fillerCorridor(Writer &writer, const Piece &piece) {
        const Blocks &b = blocks();
        for (int32_t z = 0; z < piece.mValue; ++z) {
            for (int32_t x = 0; x <= 4; ++x)
                writer.place(b.stoneBricks, x, 0, z);
            for (int32_t y = 1; y <= 3; ++y) {
                writer.place(b.stoneBricks, 0, y, z);
                writer.place(b.air, 1, y, z);
                writer.place(b.air, 2, y, z);
                writer.place(b.air, 3, y, z);
                writer.place(b.stoneBricks, 4, y, z);
            }
            for (int32_t x = 0; x <= 4; ++x)
                writer.place(b.stoneBricks, x, 4, z);
        }
    }

    void fiveCrossing(Writer &writer, const Piece &piece) {
        const Blocks &b = blocks();
        writer.stoneBox(0, 0, 0, 9, 8, 10);
        writer.door(piece.mEntryDoor, 4, 3, 0);
        if (piece.mFlags[0])
            writer.box(0, 3, 1, 0, 5, 3, b.air, b.air);
        if (piece.mFlags[3])
            writer.box(9, 3, 1, 9, 5, 3, b.air, b.air);
        if (piece.mFlags[1])
            writer.box(0, 5, 7, 0, 7, 9, b.air, b.air);
        if (piece.mFlags[2])
            writer.box(9, 5, 7, 9, 7, 9, b.air, b.air);
        writer.box(5, 1, 10, 7, 3, 10, b.air, b.air);
        writer.stoneBox(1, 2, 1, 8, 2, 6);
        writer.stoneBox(4, 1, 5, 4, 4, 9);
        writer.stoneBox(8, 1, 5, 8, 4, 9);
        writer.stoneBox(1, 4, 7, 3, 4, 9);
        writer.stoneBox(1, 3, 5, 3, 3, 6);
        writer.box(1, 3, 4, 3, 3, 4, b.brickSlab, b.air);
        writer.box(1, 4, 6, 3, 4, 6, b.brickSlab, b.air);
        writer.stoneBox(5, 1, 7, 7, 1, 8);
        writer.box(5, 1, 9, 7, 1, 9, b.brickSlab, b.air);
        writer.box(5, 2, 7, 7, 2, 7, b.brickSlab, b.air);
        writer.box(4, 5, 7, 4, 5, 9, b.brickSlab, b.air);
        writer.box(8, 5, 7, 8, 5, 9, b.brickSlab, b.air);
        writer.box(5, 5, 7, 7, 5, 9, b.smoothDoubleSlab, b.air);
        writer.place(torchFacing(SOUTH_WALL_TORCH[writer.orientation()]), 6, 5, 6);
    }

    // a left turn opens its west side once turned north or east, its east side otherwise; a right turn the reverse
    void turn(Writer &writer, const Piece &piece, bool left) {
        const Blocks &b = blocks();
        writer.stoneBox(0, 0, 0, 4, 4, 4);
        writer.door(piece.mEntryDoor, 1, 1, 0);
        const bool northOrEast =
                writer.orientation() == StructurePiece::NORTH || writer.orientation() == StructurePiece::EAST;
        if (northOrEast == left)
            writer.box(0, 1, 1, 0, 3, 3, b.air, b.air);
        else
            writer.box(4, 1, 1, 4, 3, 3, b.air, b.air);
    }

    void library(Writer &writer, const Piece &piece) {
        const Blocks &b = blocks();
        const bool tall = piece.mFlags[0];
        const int32_t orientation = writer.orientation();
        writer.stoneBox(0, 0, 0, 13, tall ? 10 : 5, 14);
        writer.door(piece.mEntryDoor, 4, 1, 0);
        writer.maybeBox(WEB_CHANCE, 2, 1, 1, 11, 4, 13, b.web);
        for (int32_t z = 1; z <= 13; ++z) {
            const BlockState &shelf = (z & 3) == 1 ? b.planks : b.bookshelf;
            writer.box(1, 1, z, 1, 4, z, shelf, shelf);
            writer.box(12, 1, z, 12, 4, z, shelf, shelf);
            if ((z & 3) == 1) {
                writer.place(torchFacing(WEST_WALL_TORCH[orientation]), 2, 3, z);
                writer.place(torchFacing(EAST_WALL_TORCH[orientation]), 11, 3, z);
            }
            if (tall) {
                writer.box(1, 6, z, 1, 9, z, shelf, shelf);
                writer.box(12, 6, z, 12, 9, z, shelf, shelf);
            }
        }
        for (int32_t z = 3; z <= 11; z += 2) {
            writer.box(3, 1, z, 4, 3, z, b.bookshelf, b.bookshelf);
            writer.box(6, 1, z, 7, 3, z, b.bookshelf, b.bookshelf);
            writer.box(9, 1, z, 10, 3, z, b.bookshelf, b.bookshelf);
        }

        if (tall) {
            writer.box(1, 5, 1, 3, 5, 13, b.planks, b.planks);
            writer.box(10, 5, 1, 12, 5, 13, b.planks, b.planks);
            writer.box(4, 5, 1, 9, 5, 2, b.planks, b.planks);
            writer.box(4, 5, 12, 9, 5, 13, b.planks, b.planks);
            writer.place(b.planks, 9, 5, 11);
            writer.place(b.planks, 8, 5, 11);
            writer.place(b.planks, 9, 5, 10);
            writer.box(3, 6, 2, 3, 6, 12, b.fence, b.fence);
            writer.box(10, 6, 2, 10, 6, 10, b.fence, b.fence);
            writer.box(4, 6, 2, 9, 6, 2, b.fence, b.fence);
            writer.box(4, 6, 12, 8, 6, 12, b.fence, b.fence);
            writer.place(b.fence, 9, 6, 11);
            writer.place(b.fence, 8, 6, 11);
            writer.place(b.fence, 9, 6, 10);
            const BlockState ladder = writer.ladder(3);
            for (int32_t y = 1; y <= 7; ++y)
                writer.place(ladder, 10, y, 13);

            const int32_t chandelier[12][3] = {{6, 9, 7}, {7, 9, 7}, {6, 8, 7}, {7, 8, 7}, {6, 7, 7}, {7, 7, 7},
                                               {5, 7, 7}, {8, 7, 7}, {6, 7, 6}, {6, 7, 8}, {7, 7, 6}, {7, 7, 8}};
            for (const auto &spot: chandelier)
                writer.place(b.fence, spot[0], spot[1], spot[2]);
            const BlockState standing = torchFacing(5);
            const int32_t candles[6][3] = {{5, 8, 7}, {8, 8, 7}, {6, 8, 6}, {6, 8, 8}, {7, 8, 6}, {7, 8, 8}};
            for (const auto &spot: candles)
                writer.place(standing, spot[0], spot[1], spot[2]);
        }

        writer.chest(3, 3, 5, 4, LIBRARY_LOOT);
        if (tall)
            writer.chest(12, 8, 1, 2, LIBRARY_LOOT);
    }

    void prisonHall(Writer &writer, const Piece &piece) {
        const Blocks &b = blocks();
        writer.stoneBox(0, 0, 0, 8, 4, 10);
        writer.door(piece.mEntryDoor, 1, 1, 0);
        writer.box(1, 1, 10, 3, 3, 10, b.air, b.air);
        writer.stoneBox(4, 1, 1, 4, 3, 1);
        writer.stoneBox(4, 1, 3, 4, 3, 3);
        writer.stoneBox(4, 1, 7, 4, 3, 7);
        writer.stoneBox(4, 1, 9, 4, 3, 9);
        writer.box(4, 1, 4, 4, 3, 6, b.ironBars, b.ironBars);
        writer.box(5, 1, 5, 7, 3, 5, b.ironBars, b.ironBars);
        writer.place(b.ironBars, 4, 3, 2);
        writer.place(b.ironBars, 4, 3, 8);
    }

    void roomCrossing(Writer &writer, const Piece &piece) {
        const Blocks &b = blocks();
        const int32_t orientation = writer.orientation();
        writer.stoneBox(0, 0, 0, 10, 6, 10);
        writer.door(piece.mEntryDoor, 4, 1, 0);
        writer.box(4, 1, 10, 6, 3, 10, b.air, b.air);
        writer.box(0, 1, 4, 0, 3, 6, b.air, b.air);
        writer.box(10, 1, 4, 10, 3, 6, b.air, b.air);
        switch (piece.mValue) {
            case 0:
                writer.place(b.stoneBricks, 5, 1, 5);
                writer.place(b.stoneBricks, 5, 2, 5);
                writer.place(b.stoneBricks, 5, 3, 5);
                writer.place(torchFacing(EAST_WALL_TORCH[orientation]), 4, 3, 5);
                writer.place(torchFacing(WEST_WALL_TORCH[orientation]), 6, 3, 5);
                writer.place(torchFacing(SOUTH_WALL_TORCH[orientation]), 5, 3, 4);
                writer.place(torchFacing(NORTH_WALL_TORCH[orientation]), 5, 3, 6);
                for (const int32_t x: {4, 6}) {
                    for (int32_t z = 4; z <= 6; ++z)
                        writer.place(b.smoothSlab, x, 1, z);
                }
                writer.place(b.smoothSlab, 5, 1, 4);
                writer.place(b.smoothSlab, 5, 1, 6);
                break;
            case 1: {
                const int32_t ring[20][2] = {{3, 3}, {7, 3}, {3, 3}, {3, 7}, {3, 4}, {7, 4}, {4, 3},
                                             {4, 7}, {3, 5}, {7, 5}, {5, 3}, {5, 7}, {3, 6}, {7, 6},
                                             {6, 3}, {6, 7}, {3, 7}, {7, 7}, {7, 3}, {7, 7}};
                for (const auto &spot: ring)
                    writer.place(b.stoneBricks, spot[0], 1, spot[1]);
                writer.place(b.stoneBricks, 5, 1, 5);
                writer.place(b.stoneBricks, 5, 2, 5);
                writer.place(b.stoneBricks, 5, 3, 5);
                writer.place(b.water, 5, 4, 5);
                break;
            }
            case 2: {
                for (int32_t z = 1; z <= 9; ++z) {
                    writer.place(b.cobblestone, 1, 3, z);
                    writer.place(b.cobblestone, 9, 3, z);
                }
                for (int32_t x = 1; x <= 9; ++x) {
                    writer.place(b.cobblestone, x, 3, 1);
                    writer.place(b.cobblestone, x, 3, 9);
                }
                const int32_t pillar[20][3] = {{5, 1, 4}, {5, 1, 6}, {5, 3, 4}, {5, 3, 6}, {4, 1, 5},
                                               {6, 1, 5}, {4, 3, 5}, {6, 3, 5}, {4, 1, 4}, {6, 1, 4},
                                               {4, 1, 6}, {6, 1, 6}, {4, 2, 4}, {6, 2, 4}, {4, 2, 6},
                                               {6, 2, 6}, {4, 3, 4}, {6, 3, 4}, {4, 3, 6}, {6, 3, 6}};
                for (const auto &spot: pillar)
                    writer.place(b.cobblestone, spot[0], spot[1], spot[2]);
                writer.place(b.torch, 5, 3, 5);
                for (int32_t z = 2; z <= 8; ++z) {
                    writer.place(b.planks, 2, 3, z);
                    writer.place(b.planks, 3, 3, z);
                    if (z <= 3 || z >= 7) {
                        writer.place(b.planks, 4, 3, z);
                        writer.place(b.planks, 5, 3, z);
                        writer.place(b.planks, 6, 3, z);
                    }
                    writer.place(b.planks, 7, 3, z);
                    writer.place(b.planks, 8, 3, z);
                }
                const BlockState ladder = writer.ladder(4);
                for (int32_t y = 1; y <= 3; ++y)
                    writer.place(ladder, 9, y, 3);
                writer.chest(3, 4, 8, 4, CROSSING_LOOT);
                break;
            }
            default:
                break;
        }
    }

    void straight(Writer &writer, const Piece &piece) {
        const Blocks &b = blocks();
        const int32_t orientation = writer.orientation();
        writer.stoneBox(0, 0, 0, 4, 4, 6);
        writer.door(piece.mEntryDoor, 1, 1, 0);
        writer.door(Door::Opening, 1, 1, 6);
        const BlockState west = torchFacing(WEST_WALL_TORCH[orientation]);
        const BlockState east = torchFacing(EAST_WALL_TORCH[orientation]);
        if (writer.chance(TORCH_CHANCE))
            writer.place(west, 1, 2, 1);
        if (writer.chance(TORCH_CHANCE))
            writer.place(east, 3, 2, 1);
        if (writer.chance(TORCH_CHANCE))
            writer.place(west, 1, 2, 5);
        if (writer.chance(TORCH_CHANCE))
            writer.place(east, 3, 2, 5);
        if (piece.mFlags[0])
            writer.box(0, 1, 2, 0, 3, 4, b.air, b.air);
        if (piece.mFlags[1])
            writer.box(4, 1, 2, 4, 3, 4, b.air, b.air);
    }

    void straightStairsDown(Writer &writer, const Piece &piece) {
        const Blocks &b = blocks();
        const BlockState stairs = writer.stairs(b.stoneStairs, 2);
        writer.stoneBox(0, 0, 0, 4, 10, 7);
        writer.door(piece.mEntryDoor, 1, 7, 0);
        writer.door(Door::Opening, 1, 1, 7);
        for (int32_t step = 0; step < 6; ++step) {
            writer.place(stairs, 1, 6 - step, 1 + step);
            writer.place(stairs, 2, 6 - step, 1 + step);
            writer.place(stairs, 3, 6 - step, 1 + step);
            if (step < 5) {
                writer.place(b.stoneBricks, 1, 5 - step, 1 + step);
                writer.place(b.stoneBricks, 2, 5 - step, 1 + step);
                writer.place(b.stoneBricks, 3, 5 - step, 1 + step);
            }
        }
    }

    void portalRoom(Writer &writer, const Piece &piece, Level &level, LevelChunk &chunk, std::mt19937 &random) {
        const Blocks &b = blocks();
        writer.stoneBox(0, 0, 0, 10, 7, 15);
        writer.door(Door::Grates, 4, 1, 0);
        writer.stoneBox(1, 6, 1, 1, 6, 14);
        writer.stoneBox(9, 6, 1, 9, 6, 14);
        writer.stoneBox(2, 6, 1, 8, 6, 2);
        writer.stoneBox(2, 6, 14, 8, 6, 14);
        writer.stoneBox(1, 1, 1, 2, 1, 4);
        writer.stoneBox(8, 1, 1, 9, 1, 4);
        writer.box(1, 1, 1, 1, 1, 3, b.lava, b.lava);
        writer.box(9, 1, 1, 9, 1, 3, b.lava, b.lava);
        writer.stoneBox(3, 1, 8, 7, 1, 12);
        writer.box(4, 1, 9, 6, 1, 11, b.lava, b.lava);
        for (int32_t z = 3; z < 14; z += 2) {
            writer.box(0, 3, z, 0, 4, z, b.ironBars, b.ironBars);
            writer.box(10, 3, z, 10, 4, z, b.ironBars, b.ironBars);
        }
        for (int32_t x = 2; x < 9; x += 2)
            writer.box(x, 3, 15, x, 4, 15, b.ironBars, b.ironBars);

        const BlockState stairs = writer.stairs(b.brickStairs, 3);
        writer.stoneBox(4, 1, 5, 6, 1, 7);
        writer.stoneBox(4, 2, 6, 6, 2, 7);
        writer.stoneBox(4, 3, 7, 6, 3, 7);
        for (int32_t x = 4; x <= 6; ++x) {
            writer.place(stairs, x, 1, 4);
            writer.place(stairs, x, 2, 5);
            writer.place(stairs, x, 3, 6);
        }

        // all twelve eyes are drawn before the first frame stands
        std::array<bool, 12> eyes{};
        for (bool &eye: eyes)
            eye = GameRandom::nextFloat(random) > FRAME_EYE_CHANCE;
        StrongholdStructure::placePortal(piece, level, chunk, eyes);
        writer.spawner(5, 3, 6);
    }
}

void StrongholdStructure::placePortal(const Piece &room, Level &level, LevelChunk &chunk,
                                      const std::array<bool, 12> &eyes) {
    std::mt19937 unused;
    Writer writer(level, chunk, room, unused);
    const Blocks &b = blocks();
    const int32_t orientation = room.getOrientation();
    // the near row faces the way the room does: the game hands the orientation to the frame as it is
    const int32_t near = orientation;
    const int32_t far = FAR_ROW_FRAMES[orientation];
    const int32_t west = WEST_COLUMN_FRAMES[orientation];
    const int32_t east = EAST_COLUMN_FRAMES[orientation];
    const int32_t frames[12][4] = {{4, 3, 8, near},  {5, 3, 8, near},  {6, 3, 8, near},  {4, 3, 12, far},
                                   {5, 3, 12, far},  {6, 3, 12, far},  {3, 3, 9, west},  {3, 3, 10, west},
                                   {3, 3, 11, west}, {7, 3, 9, east},  {7, 3, 10, east}, {7, 3, 11, east}};
    bool complete = true;
    for (size_t i = 0; i < 12; ++i) {
        const BlockState eyed = DecorationSupport::withState(b.frame, "end_portal_eye_bit", eyes[i] ? 1 : 0);
        writer.place(DecorationSupport::withState(eyed, "minecraft:cardinal_direction", CARDINALS[frames[i][3]]),
                     frames[i][0], frames[i][1], frames[i][2]);
        complete = complete && eyes[i];
    }
    if (!complete)
        return;

    for (int32_t z = 9; z <= 11; ++z) {
        for (int32_t x = 4; x <= 6; ++x)
            writer.place(b.endPortal, x, 3, z);
    }
}

void StrongholdStructure::postProcess(const Piece &piece, Level &level, LevelChunk &chunk, std::mt19937 &random) {
    Writer writer(level, chunk, piece, random);
    switch (piece.mType) {
        case Type::Start:
        case Type::StairsDown:
            stairsDown(writer, piece);
            break;
        case Type::ChestCorridor:
            chestCorridor(writer, piece);
            break;
        case Type::FillerCorridor:
            fillerCorridor(writer, piece);
            break;
        case Type::FiveCrossing:
            fiveCrossing(writer, piece);
            break;
        case Type::LeftTurn:
            turn(writer, piece, true);
            break;
        case Type::RightTurn:
            turn(writer, piece, false);
            break;
        case Type::Library:
            library(writer, piece);
            break;
        case Type::PortalRoom:
            portalRoom(writer, piece, level, chunk, random);
            break;
        case Type::PrisonHall:
            prisonHall(writer, piece);
            break;
        case Type::RoomCrossing:
            roomCrossing(writer, piece);
            break;
        case Type::Straight:
            straight(writer, piece);
            break;
        case Type::StraightStairsDown:
            straightStairsDown(writer, piece);
            break;
    }
}

void StrongholdStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();
    const int64_t seed = generator.getSeed();

    // the starts within reach: the village ones, then the grid ones of the cells the reach meets
    std::vector<StructureChunkPos> starts;
    for (const StructureChunkPos &start: *StrongholdFeature::villageStrongholds(generator)) {
        if (std::abs(start.x - chunkX) <= REACH && std::abs(start.z - chunkZ) <= REACH)
            starts.push_back(start);
    }
    const int32_t cell = StrongholdFeature::CELL_SIZE;
    for (int32_t cellX = StrongholdFeature::cellOf(chunkX - REACH); cellX <= StrongholdFeature::cellOf(chunkX + REACH);
         ++cellX) {
        for (int32_t cellZ = StrongholdFeature::cellOf(chunkZ - REACH);
             cellZ <= StrongholdFeature::cellOf(chunkZ + REACH); ++cellZ) {
            StructureChunkPos start;
            if (!StrongholdFeature::gridStronghold(seed, cellX * cell + cell / 2, cellZ * cell + cell / 2, start))
                continue;
            if (std::abs(start.x - chunkX) > REACH || std::abs(start.z - chunkZ) > REACH)
                continue;
            // the generation keeps the origin check the cell draw already made on its centre
            if ((uint32_t) start.x * (uint32_t) start.x + (uint32_t) start.z * (uint32_t) start.z < 100U)
                continue;
            starts.push_back(start);
        }
    }
    if (starts.empty())
        return;

    const ChunkStructureBounds::Box chunkBox = ChunkStructureBounds::chunkBox(chunkX, chunkZ, level.getMinY(),
                                                                              level.getMaxY());
    Box clip;
    clip.mMinX = chunkBox.minX;
    clip.mMinY = chunkBox.minY;
    clip.mMinZ = chunkBox.minZ;
    clip.mMaxX = chunkBox.maxX;
    clip.mMaxY = chunkBox.maxY;
    clip.mMaxZ = chunkBox.maxZ;

    std::mt19937 random(StructureFeature::getPostProcessSeed(seed, chunkX, chunkZ));
    for (const StructureChunkPos &start: starts) {
        const std::shared_ptr<const Layout> layout = cachedPlan(seed, start.x, start.z);
        if (!layout->mBox.intersects(clip))
            continue;

        std::vector<ChunkStructureBounds::Box> boxes;
        for (const Piece &piece: layout->mPieces) {
            const Box &box = piece.getBox();
            boxes.push_back({box.mMinX, box.mMinY, box.mMinZ, box.mMaxX, box.mMaxY, box.mMaxZ});
            if (box.intersects(clip))
                postProcess(piece, level, chunk, random);
        }
        const Box &whole = layout->mBox;
        chunk.getStructureBounds().addStart(
                NAME, 0, {whole.mMinX, whole.mMinY, whole.mMinZ, whole.mMaxX, whole.mMaxY, whole.mMaxZ}, boxes,
                chunkBox);
    }
}
