#include "World/Level/LevelGen/Structure/MansionStructure.h"

#include "World/Actor/Mob/Hostile/EvocationIllagerActor.h"
#include "World/Actor/Mob/Hostile/VindicatorActor.h"
#include "World/Actor/Mob/Passive/AllayActor.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"
#include "World/Level/LevelGen/Structure/MansionFeature.h"
#include "World/Level/LevelGen/Structure/MansionLayout.h"
#include "World/Level/LevelGen/Structure/StructureBlockTable.h"
#include "World/Level/LevelGen/Structure/StructureConnections.h"
#include "World/Level/LevelGen/Structure/StructureMaterials.h"
#include "World/Level/LevelGen/Structure/StructureTerrain.h"

#include <algorithm>
#include <bitset>
#include <map>
#include <tuple>
#include <vector>

namespace {
    enum Direction { North, East, South, West, Hidden, NoDoor };
    constexpr std::array<int32_t, 4> DX = {0, 1, 0, -1};
    constexpr std::array<int32_t, 4> DZ = {-1, 0, 1, 0};
    constexpr int32_t NEIGHBOUR_MARGIN = 2;

    struct RoomTemplate {
        int32_t x;
        int32_t z;
        int32_t turn;
        int32_t mirror = 0;
    };

    std::string roomPath(std::mt19937 &random, const char *prefix, uint32_t count) {
        return std::string("mansion/") + prefix + std::to_string(1 + random() % count) + ".nbt";
    }

    class Placement {
    public:
        Placement(Level &level, LevelChunk &chunk, StructureTerrain &terrain, int32_t x, int32_t y, int32_t z,
                  int32_t rotation, uint32_t seed)
                : mLevel(level), mChunk(chunk), mTerrain(terrain), mX(x), mY(y), mZ(z), mRotation(rotation),
                  mRandom(seed) {}

        void piece(const std::string &path, int32_t x, int32_t y, int32_t z, int32_t turn = 0, int32_t mirror = 0) {
            StructureTemplate::rotate(mRotation, x, z);
            x += mX;
            y += mY;
            z += mZ;
            const auto shape = StructureTemplate::load(path);
            if (!shape)
                return;
            const int32_t rotation = (mRotation + turn) & 3;

            int32_t farX = shape->getSizeX() - 1;
            int32_t farZ = shape->getSizeZ() - 1;
            StructureTemplate::mirror(mirror, farX, farZ);
            StructureTemplate::rotate(rotation, farX, farZ);
            const int32_t chunkX = mChunk.getX() * 16;
            const int32_t chunkZ = mChunk.getZ() * 16;
            const int32_t minX = std::max(std::min(x, x + farX), chunkX);
            const int32_t maxX = std::min(std::max(x, x + farX), chunkX + 15);
            const int32_t minZ = std::max(std::min(z, z + farZ), chunkZ);
            const int32_t maxZ = std::min(std::max(z, z + farZ), chunkZ + 15);
            if (std::min(x, x + farX) > chunkX + 15 + NEIGHBOUR_MARGIN
                || std::max(x, x + farX) < chunkX - NEIGHBOUR_MARGIN
                || std::min(z, z + farZ) > chunkZ + 15 + NEIGHBOUR_MARGIN
                || std::max(z, z + farZ) < chunkZ - NEIGHBOUR_MARGIN)
                return;

            if (y == mY) {
                for (int32_t baseZ = minZ; baseZ <= maxZ; ++baseZ) {
                    for (int32_t baseX = minX; baseX <= maxX; ++baseX)
                        mFoundation.set((baseZ & 15) * 16 + (baseX & 15));
                }
            }

            for (const auto &entry : shape->getBlocks()) {
                int32_t blockX = entry.x;
                int32_t blockZ = entry.z;
                StructureTemplate::mirror(mirror, blockX, blockZ);
                StructureTemplate::rotate(rotation, blockX, blockZ);
                blockX += x;
                blockZ += z;
                const int32_t blockY = y + entry.y;
                if (blockX < chunkX - NEIGHBOUR_MARGIN || blockX > chunkX + 15 + NEIGHBOUR_MARGIN
                    || blockZ < chunkZ - NEIGHBOUR_MARGIN || blockZ > chunkZ + 15 + NEIGHBOUR_MARGIN
                    || blockY < LevelChunk::MIN_Y || blockY > LevelChunk::MAX_Y)
                    continue;

                const bool inChunk = blockX >= chunkX && blockX < chunkX + 16 && blockZ >= chunkZ && blockZ < chunkZ + 16;
                const auto &palette = shape->getPalette()[entry.mPaletteIndex];
                if (palette.mName == "minecraft:structure_block") {
                    if (inChunk)
                        marker(entry.mData.getString("metadata"), blockX, blockY, blockZ, rotation, mirror);
                    continue;
                }
                if (palette.mName == "minecraft:structure_void")
                    continue;
                BlockState state;
                if (!StructureBlockTable::translate(palette, rotation, mirror, state))
                    continue;
                if (state.mName.ends_with("_fence") && !state.mStates.contains("minecraft:connection_east"))
                    state = StructureConnections::connected(state, false, false, false, false);
                if (!inChunk) {
                    mNeighbours.insert_or_assign(Position{blockX, blockY, blockZ}, std::move(state));
                    continue;
                }
                mChunk.setBlock(blockX & 15, blockY, blockZ & 15, state);
                if (state.mStates.contains("minecraft:connection_east")
                    || state.mStates.contains("wall_connection_type_east") || state.mStates.contains("weirdo_direction"))
                    mShapeCandidates.emplace_back(blockX, blockY, blockZ);
                if (state.mName == "minecraft:chest")
                    mLevel.getBlockActors().getOrCreate<ChestBlockActor>(Vector3i(blockX, blockY, blockZ));
            }
        }

        void shapeConnections() {
            const StructureConnections::Lookup raw = [&](int32_t x, int32_t y, int32_t z) -> BlockState {
                if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
                    return BlockState("minecraft:air");
                if ((x >> 4) == mChunk.getX() && (z >> 4) == mChunk.getZ())
                    return mChunk.getBlock(x & 15, y, z & 15);
                const auto neighbour = mNeighbours.find({x, y, z});
                return neighbour != mNeighbours.end() ? neighbour->second : mTerrain.getBlock(x, y, z);
            };
            const StructureConnections::Lookup shapedStairs = [&](int32_t x, int32_t y, int32_t z) -> BlockState {
                BlockState state = raw(x, y, z);
                return state.mStates.contains("weirdo_direction") ? StructureConnections::shaped(state, x, y, z, raw)
                                                                 : state;
            };
            std::vector<std::pair<Position, BlockState>> shaped;
            for (const auto &[x, y, z] : mShapeCandidates) {
                const BlockState state = raw(x, y, z);
                BlockState result = StructureConnections::shaped(state, x, y, z, shapedStairs);
                if (result != state)
                    shaped.emplace_back(Position{x, y, z}, std::move(result));
            }
            for (const auto &[position, state] : shaped)
                mChunk.setBlock(std::get<0>(position) & 15, std::get<1>(position), std::get<2>(position) & 15, state);
        }

        void corridors(const MansionLayout::Floor &floor, int32_t height, bool groundFloor) {
            using MansionLayout::Cell;
            const auto connects = [&](int32_t x, int32_t z) {
                if (x < 0 || x >= MansionLayout::SIZE || z < 0 || z >= MansionLayout::SIZE)
                    return false;
                return floor[z][x] == Cell::Corridor || floor[z][x] == Cell::Landing
                       || (height != 19 && x == 8 && (z == 4 || z == 5));
            };
            for (int32_t z = 0; z < MansionLayout::SIZE; ++z) {
                for (int32_t x = 0; x < MansionLayout::SIZE; ++x) {
                    if (floor[z][x] != Cell::Corridor)
                        continue;
                    const int32_t px = 8 * (x - 8);
                    const int32_t pz = 8 * (z - 4);
                    piece(groundFloor ? "mansion/corridor_floor.nbt" : "mansion/corridor_floor_2.nbt", px, height, pz);
                    if (connects(x, z - 1))
                        piece("mansion/carpet_north.nbt", px + 1, height + 1, pz);
                    if (connects(x + 1, z))
                        piece("mansion/carpet_east.nbt", px + 5, height + 1, pz + 1);
                    if (connects(x, z + 1))
                        piece(groundFloor ? "mansion/carpet_south.nbt" : "mansion/carpet_south_2.nbt",
                              px - 1, height, pz + 5);
                    if (connects(x - 1, z))
                        piece(groundFloor ? "mansion/carpet_west.nbt" : "mansion/carpet_west_2.nbt",
                              px - 1, height, pz - 1);
                }
            }
        }

        void exterior(const MansionLayout::Plan &plan) {
            walls(plan.mBase, 8, 5, 8, 4, 0, 0, 16, "mansion/wall_flat.nbt");
            walls(plan.mBase, 8, 5, 8, 4, 0, 8, 16, "mansion/wall_window.nbt");
            for (int32_t z = 0; z < MansionLayout::SIZE; ++z) {
                for (int32_t x = MansionLayout::SIZE - 1; x >= 0; --x) {
                    if (!occupied(plan.mUpper, x, z))
                        continue;
                    const int32_t px = 8 * (x - 8);
                    const int32_t pz = 8 * (z - 4);
                    piece("mansion/wall_window.nbt", px + 7, 19, pz);
                    walls(plan.mUpper, x, z, x, z, px, 19, pz + 8, "mansion/wall_window.nbt");
                    return;
                }
            }
        }

        void roofs(const MansionLayout::Plan &plan) {
            roof(plan.mBase, &plan.mUpper, 16);
            roof(plan.mUpper, nullptr, 27);
        }

        void support() {
            for (int32_t z = 0; z < 16; ++z) {
                for (int32_t x = 0; x < 16; ++x) {
                    if (mFoundation.test(z * 16 + x) && mChunk.getBlock(x, mY, z).mName != "minecraft:air")
                        foundation(x, mY - 1, z);
                }
            }
            stairway(9, 2, North);
            stairway(10, 2, North);
            stairway(9, 12, South);
            stairway(10, 12, South);
        }

        void interiors(const MansionLayout::Floor &floor, const MansionLayout::Rooms &rooms, std::mt19937 &random,
                       int32_t py) {
            using MansionLayout::Cell;
            const bool ground = py == 0;
            const char *wall = ground ? "mansion/indoors_wall.nbt" : "mansion/indoors_wall_2.nbt";
            const char *doorway = ground ? "mansion/indoors_door.nbt" : "mansion/indoors_door_2.nbt";
            for (int32_t z = 0; z < MansionLayout::SIZE; ++z) {
                for (int32_t x = 0; x < MansionLayout::SIZE; ++x) {
                    const Cell cell = floor[z][x];
                    if (cell != Cell::Room && (py != 19 || (cell != Cell::Entrance && cell != Cell::Landing)))
                        continue;
                    const auto &room = rooms[z][x];
                    const auto corridor = [&](int32_t direction) {
                        return at(floor, x + DX[direction], z + DZ[direction]) == Cell::Corridor;
                    };
                    Direction door = room.mOrigin ? Hidden : NoDoor;
                    if (room.mHasDoor) {
                        std::array<Direction, 4> doors{};
                        size_t count = 0;
                        for (int32_t d = North; d <= West; ++d) {
                            if (corridor(d))
                                doors[count++] = static_cast<Direction>(d);
                        }
                        if (count != 0)
                            door = doors[random() % count];
                    }
                    const auto sameRoom = [&](int32_t direction) {
                        const int32_t nx = x + DX[direction];
                        const int32_t nz = z + DZ[direction];
                        return nx >= 0 && nx < MansionLayout::SIZE && nz >= 0 && nz < MansionLayout::SIZE
                               && rooms[nz][nx].mId == room.mId;
                    };
                    const int32_t px = 8 * (x - 8) - 1;
                    const int32_t pz = 8 * (z - 4);
                    if (occupied(floor, x - 1, z) && !sameRoom(West))
                        piece(door == West ? doorway : wall, px, py, pz);
                    if (cell != Cell::Landing && corridor(East))
                        piece(door == East ? doorway : wall, px + 8, py, pz);
                    if (occupied(floor, x, z + 1) && !sameRoom(South))
                        piece(door == South ? doorway : wall, px + 7, py, pz + 7, 1);
                    if (cell != Cell::Landing && corridor(North))
                        piece(door == North ? doorway : wall, px + 7, py, pz - 1, 1);
                    if (door == NoDoor)
                        continue;
                    Direction adjoining = NoDoor;
                    if (room.mSize == MansionLayout::RoomSize::Square && door < Hidden) {
                        const int32_t side = (door + 1) & 3;
                        adjoining = static_cast<Direction>(sameRoom(side) ? side : (side + 2) & 3);
                    } else {
                        for (int32_t d = North; d <= West; ++d) {
                            if (sameRoom(d)) {
                                adjoining = static_cast<Direction>(d);
                                break;
                            }
                        }
                    }
                    roomPiece(room, door, adjoining, random, px, py, pz);
                }
            }
        }

    private:
        using Position = std::tuple<int32_t, int32_t, int32_t>;
        void marker(const std::string &name, int32_t x, int32_t y, int32_t z, int32_t rotation, int32_t mirror) {
            if (name.starts_with("Chest")) {
                if (mChunk.getBlock(x & 15, y, z & 15).mName == "minecraft:chest")
                    return;
                int32_t facing = North;
                if (name == "ChestEast")
                    facing = East;
                else if (name == "ChestSouth")
                    facing = South;
                else if (name == "ChestWest")
                    facing = West;
                if ((mirror == 1 && (facing & 1) == 0) || (mirror == 2 && (facing & 1) != 0))
                    facing ^= 2;
                constexpr std::array<const char *, 4> CARDINALS = {"north", "east", "south", "west"};
                Tag states = Tag::ofCompound();
                states.putString("minecraft:cardinal_direction", CARDINALS[(facing + rotation) & 3]);
                mChunk.setBlock(x & 15, y, z & 15, BlockState("minecraft:chest", states));
                mLevel.getBlockActors()
                        .getOrCreate<ChestBlockActor>(Vector3i(x, y, z))
                        .setLootTable("loot_tables/chests/woodland_mansion.json", FeatureRandom::nextInt(mRandom));
                return;
            }

            const char *identifier = nullptr;
            int32_t count = 1;
            if (name == "Warrior")
                identifier = VindicatorActor::IDENTIFIER;
            else if (name == "Mage")
                identifier = EvocationIllagerActor::IDENTIFIER;
            else if (name == "Group of Allays") {
                identifier = AllayActor::IDENTIFIER;
                count = 1 + FeatureRandom::nextInt(mRandom, 3);
            }
            if (identifier == nullptr)
                return;
            mChunk.setBlock(x & 15, y, z & 15, BlockState("minecraft:air"));
            Tag position = Tag::ofList(Tag::Type::Float);
            position.addToList(Tag::ofFloat((float) x));
            position.addToList(Tag::ofFloat((float) y));
            position.addToList(Tag::ofFloat((float) z));
            for (int32_t i = 0; i < count; ++i) {
                Tag entity = Tag::ofCompound();
                entity.putString("identifier", identifier);
                entity.put("Pos", position);
                entity.putByte("Persistent", 1);
                mLevel.addGeneratedEntity(std::move(entity));
            }
        }

        void foundation(int32_t x, int32_t y, int32_t z) {
            static const BlockState cobblestone("minecraft:cobblestone");
            for (; y >= 2; --y) {
                const BlockState &block = mChunk.getBlock(x, y, z);
                if (block.mName != "minecraft:air" && !StructureMaterials::isLiquid(block))
                    break;
                mChunk.setBlock(x, y, z, cobblestone);
            }
        }

        void stairway(int32_t x, int32_t z, Direction descending) {
            const int32_t direction = (descending + mRotation) & 3;
            StructureTemplate::rotate(mRotation, x, z);
            x += mX;
            z += mZ;
            constexpr std::array<int32_t, 4> UPHILL_STATES = {2, 1, 3, 0};
            Tag states = Tag::ofCompound();
            states.putString("minecraft:corner", "none");
            states.putByte("upside_down_bit", 0);
            states.putInt("weirdo_direction", UPHILL_STATES[direction]);
            const BlockState stair("minecraft:stone_stairs", states);
            for (int32_t y = mY - 1; y >= LevelChunk::MIN_Y; --y) {
                if ((x >> 4) != mChunk.getX() || (z >> 4) != mChunk.getZ()
                    || mChunk.getBlock(x & 15, y, z & 15).mName != "minecraft:air")
                    break;
                mChunk.setBlock(x & 15, y, z & 15, stair);
                mShapeCandidates.emplace_back(x, y, z);
                foundation(x & 15, y - 1, z & 15);
                x += DX[direction];
                z += DZ[direction];
            }
        }

        static MansionLayout::Cell at(const MansionLayout::Floor &floor, int32_t x, int32_t z) {
            using MansionLayout::Cell;
            if (x < 0 || x >= MansionLayout::SIZE || z < 0 || z >= MansionLayout::SIZE)
                return Cell::Outside;
            return floor[z][x];
        }

        static bool occupied(const MansionLayout::Floor &floor, int32_t x, int32_t z) {
            using MansionLayout::Cell;
            const Cell cell = at(floor, x, z);
            return cell != Cell::Empty && cell != Cell::Outside;
        }

        void roomPiece(const MansionLayout::RoomCell &room, Direction door, Direction adjoining, std::mt19937 &random,
                       int32_t x, int32_t y, int32_t z) {
            using MansionLayout::RoomSize;
            const bool ground = y == 0;
            if (room.mSize == RoomSize::Single) {
                std::string path = roomPath(random, ground ? "1x1_a" : "1x1_b", 5);
                constexpr std::array<RoomTemplate, 5> poses = {{
                        {1, 6, 3}, {1, 0, 0}, {7, 0, 1}, {7, 6, 2}, {1, 0, 0},
                }};
                if (door == Hidden)
                    path = roomPath(random, "1x1_as", 4);
                const auto &pose = poses[door];
                piece(path, x + pose.x, y, z + pose.z, pose.turn);
                return;
            }
            if (room.mSize == RoomSize::Square) {
                if (door == Hidden) {
                    piece("mansion/2x2_s1.nbt", x + 1, y, z);
                    return;
                }
                constexpr RoomTemplate poses[4][4] = {
                        {{}, {1, 14, 3}, {}, {7, 14, 3}},
                        {{-7, 6, 0, 1}, {}, {-7, 0, 0}, {}},
                        {{}, {1, -8, 1}, {}, {7, -8, 1}},
                        {{15, 6, 2}, {}, {15, 0, 0, 2}, {}},
                };
                const auto &pose = poses[door][adjoining];
                piece(roomPath(random, ground ? "2x2_a" : "2x2_b", ground ? 4U : 5U),
                      x + pose.x, y, z + pose.z, pose.turn, pose.mirror);
                return;
            }

            if (adjoining == NoDoor)
                return;
            if (door == Hidden) {
                if (adjoining == East || adjoining == South) {
                    const bool east = adjoining == East;
                    piece(roomPath(random, ground ? "1x2_s" : "1x2_se", ground ? 2U : 1U),
                          x + (east ? 15 : 1), y, z, east ? 1 : 0);
                }
                return;
            }
            constexpr RoomTemplate poses[4][4] = {
                    {{}, {1, 6, 3}, {7, 14, 2}, {7, 6, 1, 2}},
                    {{1, 6, 0, 1}, {}, {1, 0, 0}, {-7, 6, 3}},
                    {{1, -8, 0}, {1, 0, 1, 1}, {}, {7, 0, 1}},
                    {{7, 6, 2}, {15, 0, 1}, {7, 0, 0, 2}, {}},
            };
            const bool endDoor = (door + 2) % 4 == adjoining;
            const auto &pose = poses[door][adjoining];
            const std::string path = room.mStairs
                                             ? (endDoor ? "mansion/1x2_d_stairs.nbt" : "mansion/1x2_c_stairs.nbt")
                                             : roomPath(random, ground ? (endDoor ? "1x2_b" : "1x2_a")
                                                                       : (endDoor ? "1x2_d" : "1x2_c"),
                                                        endDoor ? 5U : (ground ? 9U : 4U));
            piece(path, x + pose.x, y, z + pose.z, pose.turn, pose.mirror);
        }

        void roof(const MansionLayout::Floor &floor, const MansionLayout::Floor *upper, int32_t py) {
            for (int32_t z = 0; z < MansionLayout::SIZE; ++z) {
                for (int32_t x = 0; x < MansionLayout::SIZE; ++x) {
                    if (!occupied(floor, x, z) || (upper && occupied(*upper, x, z)))
                        continue;
                    const int32_t px = 8 * (x - 8);
                    const int32_t pz = 8 * (z - 4);
                    piece("mansion/roof.nbt", px, py + 3, pz);
                    if (!occupied(floor, x + 1, z))
                        piece("mansion/roof_front.nbt", px + 6, py, pz);
                    if (!occupied(floor, x - 1, z))
                        piece("mansion/roof_front.nbt", px, py, pz + 7, 2);
                    if (!occupied(floor, x, z - 1))
                        piece("mansion/roof_front.nbt", px - 1, py, pz, 3);
                    if (!occupied(floor, x, z + 1))
                        piece("mansion/roof_front.nbt", px + 6, py, pz + 6, 1);
                }
            }

            for (int32_t z = 0; z < MansionLayout::SIZE; ++z) {
                for (int32_t x = 0; x < MansionLayout::SIZE; ++x) {
                    if (!occupied(floor, x, z))
                        continue;
                    const int32_t px = 8 * (x - 8);
                    const int32_t pz = 8 * (z - 4);
                    const bool eastEdge = !occupied(floor, x + 1, z);
                    const bool westEdge = !occupied(floor, x - 1, z);
                    const bool northEdge = !occupied(floor, x, z - 1);
                    const bool southEdge = !occupied(floor, x, z + 1);
                    if (upper && occupied(*upper, x, z)) {
                        if (eastEdge)
                            piece("mansion/small_wall.nbt", px + 7, py, pz);
                        if (westEdge)
                            piece("mansion/small_wall.nbt", px - 1, py, pz + 6, 2);
                        if (northEdge)
                            piece("mansion/small_wall.nbt", px, py, pz - 1, 3);
                        if (southEdge)
                            piece("mansion/small_wall.nbt", px + 6, py, pz + 7, 1);
                        if (eastEdge && northEdge)
                            piece("mansion/small_wall_corner.nbt", px + 7, py, pz - 2);
                        if (eastEdge && southEdge)
                            piece("mansion/small_wall_corner.nbt", px + 8, py, pz + 7, 3);
                        if (westEdge && northEdge)
                            piece("mansion/small_wall_corner.nbt", px - 2, py, pz - 1, 2);
                        if (westEdge && southEdge)
                            piece("mansion/small_wall_corner.nbt", px - 1, py, pz + 8, 1);
                        continue;
                    }
                    if (eastEdge) {
                        if (southEdge)
                            piece("mansion/roof_corner.nbt", px + 6, py, pz + 6);
                        else if (occupied(floor, x + 1, z + 1))
                            piece("mansion/roof_inner_corner.nbt", px + 6, py, pz + 5);
                        if (northEdge)
                            piece("mansion/roof_corner.nbt", px + 6, py, pz, 1);
                        else if (occupied(floor, x + 1, z - 1))
                            piece("mansion/roof_inner_corner.nbt", px + 9, py, pz - 2, 3);
                    }
                    if (westEdge) {
                        if (southEdge)
                            piece("mansion/roof_corner.nbt", px, py, pz + 6, 3);
                        else if (occupied(floor, x - 1, z + 1))
                            piece("mansion/roof_inner_corner.nbt", px - 3, py, pz + 8, 1);
                        if (northEdge)
                            piece("mansion/roof_corner.nbt", px, py, pz, 2);
                        else if (occupied(floor, x - 1, z - 1))
                            piece("mansion/roof_inner_corner.nbt", px, py, pz + 1, 2);
                    }
                }
            }
        }

        void walls(const MansionLayout::Floor &floor, int32_t x, int32_t z, int32_t endX, int32_t endZ,
                   int32_t px, int32_t py, int32_t pz, const char *wall) {
            constexpr std::array<int32_t, 4> DX = {0, 1, 0, -1};
            constexpr std::array<int32_t, 4> DZ = {-1, 0, 1, 0};
            int32_t turn = 0;
            int32_t direction = 2;
            const auto shift = [&](int32_t dx, int32_t dz) {
                StructureTemplate::rotate(turn, dx, dz);
                px += dx;
                pz += dz;
            };
            for (int32_t edge = 0; edge < 4 * MansionLayout::SIZE * MansionLayout::SIZE; ++edge) {
                const int32_t nx = x + DX[direction];
                const int32_t nz = z + DZ[direction];
                const int32_t left = (direction + 3) & 3;
                if (!occupied(floor, nx, nz)) {
                    shift(0, -1);
                    piece("mansion/wall_corner.nbt", px, py, pz, turn);
                    shift(6, -7);
                    turn = (turn + 1) & 3;
                    direction = (direction + 1) & 3;
                } else if (occupied(floor, nx + DX[left], nz + DZ[left])) {
                    shift(8, 6);
                    turn = (turn + 3) & 3;
                    direction = left;
                    x = nx;
                    z = nz;
                    continue;
                } else {
                    x = nx;
                    z = nz;
                }
                if (x == endX && z == endZ && direction == 2)
                    return;
                shift(7, 0);
                piece(wall, px, py, pz, turn);
                shift(-7, 8);
            }
        }

        Level &mLevel;
        LevelChunk &mChunk;
        StructureTerrain &mTerrain;
        int32_t mX, mY, mZ;
        int32_t mRotation;
        std::mt19937 mRandom;
        std::bitset<256> mFoundation;
        std::map<Position, BlockState> mNeighbours;
        std::vector<Position> mShapeCandidates;
    };
}

void MansionStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    const StructureFeature *feature = StructureFeature::get("mansion");
    if (feature == nullptr)
        return;

    constexpr int32_t REACH = 7;
    const int32_t x = chunk.getX();
    const int32_t z = chunk.getZ();
    for (const StructureChunkPos &start : feature->getCandidatesIn(generator.getSeed(), x - REACH, z - REACH,
                                                                    x + REACH, z + REACH)) {
        if (!feature->isStartChunk(generator, start.x, start.z))
            continue;

        auto random = feature->createStartRandom(generator.getSeed(), start.x, start.z);
        const int32_t rotation = (int32_t) (random() % 4);
        StructureTerrain terrain(generator, level.getTerrainCache());
        const int32_t y = MansionFeature::getBaseY(terrain, start.x, start.z, rotation);
        const auto plan = MansionLayout::build(random);
        Placement placement(level, chunk, terrain, start.x * 16 + 8, y, start.z * 16 + 8, rotation,
                            StructureFeature::getPostProcessSeed(generator.getSeed(), x, z));
        placement.piece("mansion/entrance.nbt", -9, 0, 0);
        placement.exterior(plan);
        placement.roofs(plan);
        placement.corridors(plan.mBase, 0, true);
        placement.corridors(plan.mBase, 8, false);
        placement.corridors(plan.mUpper, 19, false);
        placement.interiors(plan.mBase, plan.mRooms[0], random, 0);
        placement.interiors(plan.mBase, plan.mRooms[1], random, 8);
        placement.interiors(plan.mUpper, plan.mRooms[2], random, 19);
        placement.support();
        placement.shapeConnections();
    }
}
