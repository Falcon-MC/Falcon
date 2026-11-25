#include "World/Level/LevelGen/Structure/EndCityStructure.h"

#include "World/Actor/Mob/Hostile/ShulkerActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Structure/EndCityFeature.h"
#include "World/Level/LevelGen/Structure/StructureBlockTable.h"
#include "World/Level/LevelGen/Structure/StructureConnections.h"
#include "World/Level/LevelGen/Structure/StructureMarkers.h"
#include "World/Level/LevelGen/Structure/StructureTemplate.h"

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <tuple>

namespace {
    using Piece = EndCityStructure::Piece;
    using Pieces = std::vector<Piece>;

    const char *const NAME = "minecraft:end_city";
    const char *const LOOT_TABLE = "loot_tables/chests/end_city_treasure.json";
    // a city past this depth stops growing
    const int32_t MAX_DEPTH = 8;
    // the farthest a city reaches from its start chunk, ship included
    const int32_t REACH = 14;
    const int32_t MIN_Y = 0;
    const int32_t MAX_Y = 255;

    struct Offset {
        int32_t mQuarterTurns;
        int32_t x;
        int32_t y;
        int32_t z;
    };

    // the bridge ends a tower or a fat tower can grow, each turned on top of the parent
    const std::array<Offset, 4> TOWER_BRIDGES = {{{0, 1, -1, 0}, {1, 6, -1, 1}, {3, 0, -1, 5}, {2, 5, -1, 6}}};
    const std::array<Offset, 4> FAT_TOWER_BRIDGES = {{{0, 4, -1, 0}, {1, 12, -1, 4}, {3, 0, -1, 8}, {2, 8, -1, 12}}};
    // the facing_direction of the elytra frame for turns 1 to 3, south without a turn
    const std::array<int32_t, 4> FRAME_FACING = {3, 4, 2, 5};

    std::shared_ptr<const StructureTemplate> shapeOf(const std::string &name) {
        return StructureTemplate::load("endcity/" + name + ".nbt");
    }

    int32_t nextInt(std::mt19937 &random, uint32_t bound) {
        return bound == 0 ? 0 : (int32_t) (random() % bound);
    }

    bool nextBoolean(std::mt19937 &random) {
        return (random() & 0x8000000u) != 0;
    }

    Piece makePiece(const std::string &name, int32_t x, int32_t y, int32_t z, int32_t quarterTurns, bool overwrite) {
        Piece piece;
        piece.mTemplate = name;
        piece.x = x;
        piece.y = y;
        piece.z = z;
        piece.mQuarterTurns = quarterTurns & 3;
        piece.mOverwrite = overwrite;
        int32_t farX = 0;
        int32_t farY = 0;
        int32_t farZ = 0;
        if (const auto shape = shapeOf(name)) {
            farX = shape->getSizeX() - 1;
            farY = shape->getSizeY() - 1;
            farZ = shape->getSizeZ() - 1;
        }
        StructureTemplate::rotate(piece.mQuarterTurns, farX, farZ);
        piece.mBox.mMinX = x + std::min(0, farX);
        piece.mBox.mMaxX = x + std::max(0, farX);
        piece.mBox.mMinY = y;
        piece.mBox.mMaxY = y + farY;
        piece.mBox.mMinZ = z + std::min(0, farZ);
        piece.mBox.mMaxZ = z + std::max(0, farZ);
        return piece;
    }

    // the offset is turned with the parent; the new piece keeps its own turn
    Piece addPiece(Pieces &pieces, const Piece &parent, int32_t x, int32_t y, int32_t z, const std::string &name,
                   int32_t quarterTurns, bool overwrite) {
        StructureTemplate::rotate(parent.mQuarterTurns, x, z);
        pieces.push_back(makePiece(name, parent.x + x, parent.y + y, parent.z + z, quarterTurns, overwrite));
        return pieces.back();
    }

    enum class Section { Tower, FatTower, TowerBridge, HouseTower };

    class Planner {
    public:
        explicit Planner(std::mt19937 &random) : mRandom(random) {}

        // the generated pieces join the city only when none of them overlaps a piece of another branch
        bool children(Section section, int32_t depth, const Piece &parent, const std::optional<Offset> &position,
                      Pieces &pieces) {
            if (depth > MAX_DEPTH)
                return false;
            Pieces generated;
            if (!generate(section, depth, parent, position, generated))
                return false;

            const auto id = (int32_t) (mRandom() >> 1);
            for (Piece &piece: generated) {
                piece.mDepth = id;
                const auto collision = std::find_if(pieces.begin(), pieces.end(), [&](const Piece &other) {
                    return other.mBox.intersects(piece.mBox);
                });
                if (collision != pieces.end() && collision->mDepth != parent.mDepth)
                    return false;
            }
            pieces.insert(pieces.end(), generated.begin(), generated.end());
            return true;
        }

    private:
        bool generate(Section section, int32_t depth, const Piece &parent, const std::optional<Offset> &position,
                      Pieces &pieces) {
            switch (section) {
                case Section::Tower:
                    return tower(depth, parent, pieces);
                case Section::FatTower:
                    return fatTower(depth, parent, pieces);
                case Section::TowerBridge:
                    return towerBridge(depth, parent, pieces);
                case Section::HouseTower:
                    return houseTower(depth, parent, *position, pieces);
            }
            return false;
        }

        bool tower(int32_t depth, const Piece &parent, Pieces &pieces) {
            const int32_t turns = parent.mQuarterTurns;
            const int32_t baseX = nextInt(mRandom, 2) + 3;
            const int32_t baseZ = nextInt(mRandom, 2) + 3;
            Piece piece = addPiece(pieces, parent, baseX, -3, baseZ, "tower_base", turns, true);
            piece = addPiece(pieces, piece, 0, 7, 0, "tower_piece", turns, true);
            std::optional<Piece> bridgePiece;
            if (nextInt(mRandom, 3) == 0)
                bridgePiece = piece;
            const int32_t floors = 1 + nextInt(mRandom, 3);
            for (int32_t floor = 0; floor < floors; ++floor) {
                piece = addPiece(pieces, piece, 0, 4, 0, "tower_piece", turns, true);
                if (floor < floors - 1 && nextBoolean(mRandom))
                    bridgePiece = piece;
            }

            if (bridgePiece.has_value()) {
                for (const Offset &bridge: TOWER_BRIDGES) {
                    if (!nextBoolean(mRandom))
                        continue;
                    const Piece end = addPiece(pieces, *bridgePiece, bridge.x, bridge.y, bridge.z, "bridge_end",
                                               turns + bridge.mQuarterTurns, true);
                    children(Section::TowerBridge, depth + 1, end, std::nullopt, pieces);
                }
            } else if (depth != MAX_DEPTH - 1) {
                return children(Section::FatTower, depth + 1, piece, std::nullopt, pieces);
            }
            addPiece(pieces, piece, -1, 4, -1, "tower_top", turns, true);
            return true;
        }

        bool fatTower(int32_t depth, const Piece &parent, Pieces &pieces) {
            const int32_t turns = parent.mQuarterTurns;
            Piece piece = addPiece(pieces, parent, -3, 4, -3, "fat_tower_base", turns, true);
            piece = addPiece(pieces, piece, 0, 4, 0, "fat_tower_middle", turns, true);
            for (int32_t floor = 0; floor < 2 && nextInt(mRandom, 3) != 0; ++floor) {
                piece = addPiece(pieces, piece, 0, 8, 0, "fat_tower_middle", turns, true);
                for (const Offset &bridge: FAT_TOWER_BRIDGES) {
                    if (!nextBoolean(mRandom))
                        continue;
                    const Piece end = addPiece(pieces, piece, bridge.x, bridge.y, bridge.z, "bridge_end",
                                               turns + bridge.mQuarterTurns, true);
                    children(Section::TowerBridge, depth + 1, end, std::nullopt, pieces);
                }
            }
            addPiece(pieces, piece, -2, 8, -2, "fat_tower_top", turns, true);
            return true;
        }

        bool towerBridge(int32_t depth, const Piece &parent, Pieces &pieces) {
            const int32_t turns = parent.mQuarterTurns;
            const int32_t length = nextInt(mRandom, 4) + 1;
            Piece piece = addPiece(pieces, parent, 0, 0, -4, "bridge_piece", turns, true);
            pieces.back().mDepth = -1;
            piece.mDepth = -1;
            int32_t y = 0;
            for (int32_t step = 0; step < length; ++step) {
                if (nextBoolean(mRandom)) {
                    piece = addPiece(pieces, piece, 0, y, -4, "bridge_piece", turns, true);
                    y = 0;
                } else {
                    if (nextBoolean(mRandom))
                        piece = addPiece(pieces, piece, 0, y, -4, "bridge_steep_stairs", turns, true);
                    else
                        piece = addPiece(pieces, piece, 0, y, -8, "bridge_gentle_stairs", turns, true);
                    y = 4;
                }
            }

            if (!mShipCreated && nextInt(mRandom, (uint32_t) (10 - depth)) == 0) {
                const int32_t shipX = nextInt(mRandom, 8) - 8;
                const int32_t shipZ = nextInt(mRandom, 10) - 70;
                addPiece(pieces, piece, shipX, y, shipZ, "ship", turns, true);
                mShipCreated = true;
            } else if (!children(Section::HouseTower, depth + 1, piece, Offset{0, -3, y + 1, -11}, pieces)) {
                return false;
            }

            addPiece(pieces, piece, 4, y, 0, "bridge_end", turns + 2, true);
            pieces.back().mDepth = -1;
            return true;
        }

        bool houseTower(int32_t depth, const Piece &parent, const Offset &position, Pieces &pieces) {
            if (depth > MAX_DEPTH)
                return false;
            const int32_t turns = parent.mQuarterTurns;
            Piece piece = addPiece(pieces, parent, position.x, position.y, position.z, "base_floor", turns, true);
            switch (nextInt(mRandom, 3)) {
                case 0:
                    addPiece(pieces, piece, -1, 4, -1, "base_roof", turns, true);
                    break;
                case 1:
                    piece = addPiece(pieces, piece, -1, 0, -1, "second_floor_2", turns, false);
                    piece = addPiece(pieces, piece, -1, 8, -1, "second_roof", turns, false);
                    children(Section::Tower, depth + 1, piece, std::nullopt, pieces);
                    break;
                default:
                    piece = addPiece(pieces, piece, -1, 0, -1, "second_floor_2", turns, false);
                    piece = addPiece(pieces, piece, -1, 4, -1, "third_floor_c", turns, false);
                    piece = addPiece(pieces, piece, -1, 8, -1, "third_roof", turns, true);
                    children(Section::Tower, depth + 1, piece, std::nullopt, pieces);
                    break;
            }
            return true;
        }

        std::mt19937 &mRandom;
        bool mShipCreated = false;
    };

    using Position = std::tuple<int32_t, int32_t, int32_t>;

    // calls visit(x, y, z, palette entry, block data) for each template block of the piece the world keeps
    template<typename Visit>
    void forEachBlock(const Piece &piece, Visit &&visit) {
        const auto shape = shapeOf(piece.mTemplate);
        if (shape == nullptr)
            return;
        for (const StructureTemplate::BlockEntry &entry: shape->getBlocks()) {
            const StructureTemplate::PaletteEntry &palette = shape->getPalette()[(size_t) entry.mPaletteIndex];
            if (palette.mName == "minecraft:structure_void" || (!piece.mOverwrite && palette.mName == "minecraft:air"))
                continue;
            int32_t x = entry.x;
            int32_t z = entry.z;
            StructureTemplate::rotate(piece.mQuarterTurns, x, z);
            visit(piece.x + x, piece.y + entry.y, piece.z + z, palette, entry.mData);
        }
    }

    struct Plan {
        Pieces mPieces;
        StructurePiece::Box mBox;
        // the stairs left standing once every piece is down: their corners look across pieces and chunks
        std::map<Position, BlockState> mStairs;
    };

    std::map<Position, BlockState> stairsOf(const Pieces &pieces) {
        std::map<Position, BlockState> stairs;
        for (const Piece &piece: pieces) {
            forEachBlock(piece, [&](int32_t x, int32_t y, int32_t z, const StructureTemplate::PaletteEntry &palette,
                                    const Tag &) {
                BlockState state;
                if (palette.mName != "minecraft:structure_block"
                    && StructureBlockTable::translate(palette, piece.mQuarterTurns, state)
                    && state.mStates.contains("weirdo_direction"))
                    stairs[{x, y, z}] = state;
                else
                    stairs.erase({x, y, z});
            });
        }
        return stairs;
    }

    std::shared_ptr<const Plan> planAt(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) {
        static std::mutex mutex;
        static std::map<std::tuple<int64_t, int32_t, int32_t>, std::shared_ptr<const Plan>> cache;
        const auto key = std::make_tuple(generator.getSeed(), chunkX, chunkZ);
        {
            std::lock_guard<std::mutex> lock(mutex);
            const auto found = cache.find(key);
            if (found != cache.end())
                return found->second;
        }

        std::mt19937 turnRandom = EndCityFeature::createCityRandom(chunkX, chunkZ);
        const auto quarterTurns = (int32_t) (turnRandom() & 3);
        const int32_t y = EndCityFeature::getBaseY(generator, chunkX, chunkZ, quarterTurns);
        // the pieces draw from the region random the start chunk came out of
        std::mt19937 random = StructureFeature::get("end_city")->createStartRandom(generator.getSeed(), chunkX, chunkZ);
        auto plan = std::make_shared<Plan>();
        plan->mPieces = EndCityStructure::plan(chunkX * 16 + 8, y, chunkZ * 16 + 8, quarterTurns, random);
        plan->mBox = plan->mPieces.front().mBox;
        for (const Piece &piece: plan->mPieces) {
            plan->mBox.mMinX = std::min(plan->mBox.mMinX, piece.mBox.mMinX);
            plan->mBox.mMinY = std::min(plan->mBox.mMinY, piece.mBox.mMinY);
            plan->mBox.mMinZ = std::min(plan->mBox.mMinZ, piece.mBox.mMinZ);
            plan->mBox.mMaxX = std::max(plan->mBox.mMaxX, piece.mBox.mMaxX);
            plan->mBox.mMaxY = std::max(plan->mBox.mMaxY, piece.mBox.mMaxY);
            plan->mBox.mMaxZ = std::max(plan->mBox.mMaxZ, piece.mBox.mMaxZ);
        }
        plan->mStairs = stairsOf(plan->mPieces);

        std::lock_guard<std::mutex> lock(mutex);
        // the cities near the chunks being generated; a far away walk starts over
        if (cache.size() > 256)
            cache.clear();
        return cache.emplace(key, plan).first->second;
    }

    Tag positionTag(const char *id, const Vector3i &position) {
        Tag data = Tag::ofCompound();
        data.putString("id", id);
        data.putInt("x", position.x);
        data.putInt("y", position.y);
        data.putInt("z", position.z);
        return data;
    }

    Tag itemTag(const char *name, int16_t damage, int8_t slot) {
        Tag item = Tag::ofCompound();
        item.putString("Name", name);
        item.putByte("Count", 1);
        item.putShort("Damage", damage);
        item.putByte("WasPickedUp", 0);
        if (slot >= 0)
            item.putByte("Slot", slot);
        return item;
    }

    // the ship's brewing stand holds two strong healing potions, in its left and right slots
    Tag brewingStandTag(const Vector3i &position, const Tag &data) {
        constexpr int16_t STRONG_HEALING = 22;
        Tag stand = positionTag("BrewingStand", position);
        Tag items = Tag::ofList(Tag::Type::Compound);
        if (const Tag *source = data.get("Items"); source != nullptr && source->isList()) {
            for (const Tag &item: source->getList())
                items.addToList(itemTag("minecraft:potion", STRONG_HEALING, (int8_t) item.getByte("Slot", 0)));
        }
        stand.put("Items", items);
        return stand;
    }

    Tag bannerTag(const Vector3i &position, const Tag &data) {
        Tag banner = positionTag("Banner", position);
        banner.putInt("Base", data.getInt("Base", 0));
        banner.putInt("Type", 0);
        Tag patterns = Tag::ofList(Tag::Type::Compound);
        if (const Tag *source = data.get("Patterns"); source != nullptr && source->isList()) {
            for (const Tag &entry: source->getList()) {
                Tag pattern = Tag::ofCompound();
                pattern.putString("Pattern", entry.getString("Pattern"));
                pattern.putInt("Color", entry.getInt("Color", 0));
                patterns.addToList(pattern);
            }
        }
        banner.put("Patterns", patterns);
        return banner;
    }

    // the old skull keeps its kind in the block actor; a wall head carries the side it faces
    std::optional<BlockState> skull(const StructureTemplate::PaletteEntry &palette, int32_t quarterTurns,
                                    const Tag &data) {
        static const std::array<const char *, 7> HEADS = {
                "minecraft:skeleton_skull", "minecraft:wither_skeleton_skull", "minecraft:zombie_head",
                "minecraft:player_head",    "minecraft:creeper_head",          "minecraft:dragon_head",
                "minecraft:piglin_head"};
        // clockwise from south, with their facing_direction
        static const std::array<std::pair<const char *, int32_t>, 4> SIDES = {
                {{"south", 3}, {"west", 4}, {"north", 2}, {"east", 5}}};
        const int32_t type = data.getByte("SkullType", 0);
        const auto facing = palette.mProperties.find("facing");
        if (type < 0 || type >= (int32_t) HEADS.size() || facing == palette.mProperties.end())
            return std::nullopt;
        const auto side = std::find_if(SIDES.begin(), SIDES.end(), [&](const auto &entry) {
            return facing->second == entry.first;
        });
        if (side == SIDES.end())
            return std::nullopt;
        Tag states = Tag::ofCompound();
        states.putInt("facing_direction", SIDES[(size_t) ((side - SIDES.begin() + quarterTurns) & 3)].second);
        return BlockState(HEADS[(size_t) type], states);
    }

    class Placer {
    public:
        Placer(const ChunkGenerator &generator, Level &level, LevelChunk &chunk)
                : mLevel(level), mChunk(chunk), mChunkX(chunk.getX() * 16), mChunkZ(chunk.getZ() * 16),
                  mRandom(StructureFeature::createPostProcessRandom(generator.getSeed(), chunk.getX(), chunk.getZ())) {}

        bool overlaps(const StructurePiece::Box &box) const {
            return box.mMaxX >= mChunkX && box.mMinX <= mChunkX + 15 && box.mMaxZ >= mChunkZ
                   && box.mMinZ <= mChunkZ + 15;
        }

        bool inChunk(int32_t x, int32_t y, int32_t z) const {
            return x >= mChunkX && x <= mChunkX + 15 && z >= mChunkZ && z <= mChunkZ + 15 && y >= MIN_Y
                   && y <= MAX_Y;
        }

        void piece(const Piece &piece, const std::map<Position, BlockState> &stairs) {
            if (!overlaps(piece.mBox))
                return;

            const auto stairAt = [&](int32_t x, int32_t y, int32_t z) {
                const auto found = stairs.find({x, y, z});
                return found == stairs.end() ? BlockState("minecraft:air") : found->second;
            };
            std::vector<StructureMarkers::Marker> markers;
            forEachBlock(piece, [&](int32_t x, int32_t y, int32_t z, const StructureTemplate::PaletteEntry &palette,
                                    const Tag &data) {
                if (!inChunk(x, y, z))
                    return;
                if (palette.mName == "minecraft:structure_block") {
                    markers.push_back({x, y, z, data.getString("metadata")});
                    return;
                }

                BlockState state;
                if (palette.mName == "minecraft:skull") {
                    const std::optional<BlockState> head = skull(palette, piece.mQuarterTurns, data);
                    if (!head.has_value())
                        return;
                    state = *head;
                } else if (!StructureBlockTable::translate(palette, piece.mQuarterTurns, state)) {
                    return;
                }
                mChunk.setBlock(x & 15, y, z & 15, StructureConnections::shaped(state, x, y, z, stairAt));
                // placing a chest draws a seed of its own; the marker above replaces it
                if (palette.mName == "minecraft:chest")
                    mRandom();
                blockActor(palette.mName, Vector3i(x, y, z), data);
            });

            for (const StructureMarkers::Marker &marker: StructureMarkers::inGameOrder(markers))
                dataMarker(piece, marker);
        }

    private:
        void blockActor(const std::string &name, const Vector3i &position, const Tag &data) {
            BlockActorStore &actors = mLevel.getBlockActors();
            if (name == "minecraft:chest")
                actors.getOrCreate<ChestBlockActor>(position);
            else if (name == "minecraft:ender_chest")
                actors.keepGenerated(positionTag("EnderChest", position));
            else if (name == "minecraft:brewing_stand")
                actors.keepGenerated(brewingStandTag(position, data));
            else if (name == "minecraft:wall_banner")
                actors.keepGenerated(bannerTag(position, data));
            else if (name == "minecraft:skull")
                actors.keepGenerated(positionTag("Skull", position));
        }

        void dataMarker(const Piece &piece, const StructureMarkers::Marker &marker) {
            if (marker.mText == "Chest") {
                if (marker.y - 1 < MIN_Y)
                    return;
                const Vector3i below(marker.x, marker.y - 1, marker.z);
                if (mChunk.getBlock(below.x & 15, below.y, below.z & 15).mName != "minecraft:chest")
                    return;
                const auto seed = (int32_t) (mRandom() >> 1);
                mLevel.getBlockActors().getOrCreate<ChestBlockActor>(below).setLootTable(LOOT_TABLE, seed);
            } else if (marker.mText == "Sentry") {
                Tag location = Tag::ofList(Tag::Type::Float);
                location.addToList(Tag::ofFloat((float) marker.x));
                location.addToList(Tag::ofFloat((float) marker.y));
                location.addToList(Tag::ofFloat((float) marker.z));
                Tag shulker = Tag::ofCompound();
                shulker.putString("identifier", ShulkerActor::IDENTIFIER);
                shulker.put("Pos", location);
                mLevel.addGeneratedEntity(shulker);
            } else if (marker.mText == "Elytra") {
                const Tag *defaults = BlockPaletteRegistry::getInstance().getDefaultStates("minecraft:frame");
                if (defaults == nullptr)
                    return;
                Tag states = *defaults;
                states.putInt("facing_direction", FRAME_FACING[piece.mQuarterTurns & 3]);
                mChunk.setBlock(marker.x & 15, marker.y, marker.z & 15, BlockState("minecraft:frame", states));
                Tag frame = positionTag("ItemFrame", Vector3i(marker.x, marker.y, marker.z));
                frame.put("Item", itemTag("minecraft:elytra", 0, -1));
                frame.putByte("ItemRotation", 0);
                frame.putFloat("ItemDropChance", 1.0f);
                mLevel.getBlockActors().keepGenerated(frame);
            }
        }

        Level &mLevel;
        LevelChunk &mChunk;
        int32_t mChunkX;
        int32_t mChunkZ;
        std::mt19937 mRandom;
    };

    ChunkStructureBounds::Box boundsOf(const StructurePiece::Box &box) {
        return {box.mMinX, box.mMinY, box.mMinZ, box.mMaxX, box.mMaxY, box.mMaxZ};
    }
}

std::vector<EndCityStructure::Piece> EndCityStructure::plan(int32_t x, int32_t y, int32_t z, int32_t quarterTurns,
                                                            std::mt19937 &random) {
    Pieces pieces;
    pieces.push_back(makePiece("base_floor", x, y, z, quarterTurns, true));
    Piece piece = addPiece(pieces, pieces.back(), -1, 0, -1, "second_floor", quarterTurns, false);
    piece = addPiece(pieces, piece, -1, 4, -1, "third_floor", quarterTurns, false);
    piece = addPiece(pieces, piece, -1, 8, -1, "third_roof", quarterTurns, true);
    Planner(random).children(Section::Tower, 1, piece, std::nullopt, pieces);
    return pieces;
}

void EndCityStructure::place(const ChunkGenerator &generator, Level &level, LevelChunk &chunk) {
    const StructureFeature *feature = StructureFeature::get("end_city");
    if (feature == nullptr)
        return;

    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();
    Placer placer(generator, level, chunk);
    for (const StructureChunkPos &start: feature->getCandidatesIn(generator.getSeed(), chunkX - REACH,
                                                                   chunkZ - REACH, chunkX + REACH, chunkZ + REACH)) {
        if (!feature->isStartChunk(generator, start.x, start.z))
            continue;
        const std::shared_ptr<const Plan> plan = planAt(generator, start.x, start.z);
        if (!placer.overlaps(plan->mBox))
            continue;

        std::vector<ChunkStructureBounds::Box> boxes;
        for (const Piece &piece: plan->mPieces) {
            placer.piece(piece, plan->mStairs);
            boxes.push_back(boundsOf(piece.mBox));
        }
        chunk.getStructureBounds().addStart(NAME, 0, boundsOf(plan->mBox), boxes,
                                            ChunkStructureBounds::chunkBox(chunkX, chunkZ, MIN_Y, MAX_Y));
    }
}
