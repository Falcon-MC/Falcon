#include "World/Level/Sculk/SculkSpreader.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/PlacementHelpers.h"
#include "World/Level/Level.h"
#include "World/Level/Particle/GenericLevelEventParticle.h"
#include "World/Level/Sculk/SculkWorld.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <tuple>
#include <unordered_set>

namespace {
    const char *TAG_CURSORS = "cursors";
    const int32_t EVENT_SCULK_CHARGE = 2037;
    const int32_t EVENT_SCULK_CHARGE_POP = 2038;
    const char *const SOUND_SPREAD = "block.sculk.spread";
    // a world generation cursor stops this far from its origin, squared
    const int64_t WORLD_GENERATION_REACH_SQUARED = 225;

    // the 18 neighbours that are not corners, in the game's order
    const std::array<Vector3i, 18> NON_CORNER_NEIGHBOURS = {
            Vector3i(-1, -1, 0), Vector3i(0, -1, -1), Vector3i(0, -1, 0), Vector3i(0, -1, 1),  Vector3i(1, -1, 0),
            Vector3i(-1, 0, -1), Vector3i(-1, 0, 0),  Vector3i(-1, 0, 1), Vector3i(0, 0, -1),  Vector3i(0, 0, 1),
            Vector3i(1, 0, -1),  Vector3i(1, 0, 0),   Vector3i(1, 0, 1),  Vector3i(-1, 1, 0),  Vector3i(0, 1, -1),
            Vector3i(0, 1, 0),   Vector3i(0, 1, 1),   Vector3i(1, 1, 0)};

    // what sculk grows over: the game's list for the catalyst
    const std::unordered_set<std::string> &replaceableBlocks() {
        static const std::unordered_set<std::string> blocks = {
                "minecraft:stone", "minecraft:granite", "minecraft:diorite", "minecraft:andesite", "minecraft:tuff",
                "minecraft:deepslate", "minecraft:grass_block", "minecraft:podzol", "minecraft:dirt",
                "minecraft:coarse_dirt", "minecraft:mycelium", "minecraft:dirt_with_roots", "minecraft:moss_block",
                "minecraft:pale_moss_block", "minecraft:hardened_clay", "minecraft:white_terracotta",
                "minecraft:orange_terracotta", "minecraft:magenta_terracotta", "minecraft:light_blue_terracotta",
                "minecraft:yellow_terracotta", "minecraft:lime_terracotta", "minecraft:pink_terracotta",
                "minecraft:gray_terracotta", "minecraft:light_gray_terracotta", "minecraft:cyan_terracotta",
                "minecraft:purple_terracotta", "minecraft:blue_terracotta", "minecraft:brown_terracotta",
                "minecraft:green_terracotta", "minecraft:red_terracotta", "minecraft:black_terracotta",
                "minecraft:crimson_nylium", "minecraft:warped_nylium", "minecraft:netherrack", "minecraft:basalt",
                "minecraft:blackstone", "minecraft:sand", "minecraft:red_sand", "minecraft:gravel",
                "minecraft:soul_sand", "minecraft:soul_soil", "minecraft:calcite", "minecraft:smooth_basalt",
                "minecraft:clay", "minecraft:dripstone_block", "minecraft:end_stone", "minecraft:sandstone",
                "minecraft:red_sandstone", "minecraft:mud", "minecraft:muddy_mangrove_roots", "minecraft:sulfur",
                "minecraft:cinnabar"};
        return blocks;
    }

    // world generation also grows over the deepslate variants
    const std::unordered_set<std::string> &worldGenerationExtras() {
        static const std::unordered_set<std::string> blocks = {
                "minecraft:deepslate_bricks", "minecraft:deepslate_tiles", "minecraft:cobbled_deepslate",
                "minecraft:cracked_deepslate_bricks", "minecraft:cracked_deepslate_tiles",
                "minecraft:polished_deepslate"};
        return blocks;
    }

    Vector3i add(const Vector3i &a, const Vector3i &b) {
        return Vector3i(a.x + b.x, a.y + b.y, a.z + b.z);
    }


    int32_t opposite(int32_t face) {
        return face ^ 1;
    }

    Vector3i faceStep(int32_t face) {
        switch (face) {
            case 0:
                return Vector3i(0, -1, 0);
            case 1:
                return Vector3i(0, 1, 0);
            case 2:
                return Vector3i(0, 0, -1);
            case 3:
                return Vector3i(0, 0, 1);
            case 4:
                return Vector3i(-1, 0, 0);
            default:
                return Vector3i(1, 0, 0);
        }
    }

    // the step from one block to its neighbour: faces down/up, north/south, west/east
    int32_t verticalFace(int32_t fromY, int32_t toY) {
        return fromY < toY ? 1 : 0;
    }

    int32_t depthFace(int32_t fromZ, int32_t toZ) {
        return fromZ < toZ ? 3 : 2;
    }

    int32_t widthFace(int32_t fromX, int32_t toX) {
        return fromX < toX ? 5 : 4;
    }

    // nothing sturdy blocks the way out of from through face
    bool isUnobstructed(SculkWorld &world, const Vector3i &from, int32_t face) {
        const Vector3i next = add(from, faceStep(face));
        return !PlacementHelpers::hasSolidFace(world.getBlock(next), opposite(face));
    }

    // a diagonal step needs one of its two sides open
    bool isMovementUnobstructed(SculkWorld &world, const Vector3i &from, const Vector3i &to) {
        const int32_t manhattan = std::abs(to.x - from.x) + std::abs(to.y - from.y) + std::abs(to.z - from.z);
        if (manhattan == 1)
            return true;
        int32_t first = 0;
        int32_t second = 0;
        if (from.x == to.x) {
            first = verticalFace(from.y, to.y);
            second = depthFace(from.z, to.z);
        } else {
            first = widthFace(from.x, to.x);
            second = from.y == to.y ? depthFace(from.z, to.z) : verticalFace(from.y, to.y);
        }
        return isUnobstructed(world, from, first) || isUnobstructed(world, from, second);
    }

    Tag chargeEvent(const Vector3i &position) {
        Tag data = Tag::ofCompound();
        data.putInt("x", position.x);
        data.putInt("y", position.y);
        data.putInt("z", position.z);
        return data;
    }

    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }
}

// the delays and the facing are shorts on disk
Tag SculkChargeCursor::save() const {
    Tag data = Tag::ofCompound();
    data.putInt("x", mPosition.x);
    data.putInt("y", mPosition.y);
    data.putInt("z", mPosition.z);
    data.putShort("charge", (int16_t) mCharge);
    data.putShort("update", (int16_t) mUpdateDelay);
    data.putShort("decay", (int16_t) mDecayDelay);
    data.putShort("facing", (int16_t) mFacing);
    return data;
}

SculkChargeCursor SculkChargeCursor::load(const Tag &data) {
    SculkChargeCursor cursor;
    cursor.mPosition = Vector3i(data.getInt("x"), data.getInt("y"), data.getInt("z"));
    cursor.mCharge = data.getShort("charge");
    cursor.mUpdateDelay = data.getShort("update");
    cursor.mDecayDelay = data.getShort("decay");
    cursor.mFacing = data.getShort("facing");
    return cursor;
}

void SculkSpreader::addCursors(const Vector3i &position, int32_t charge) {
    while (charge > 0) {
        const int32_t part = std::min(charge, MAX_CHARGE);
        if (mCursors.size() < MAX_CURSORS) {
            SculkChargeCursor cursor;
            cursor.mPosition = position;
            cursor.mCharge = part;
            mCursors.push_back(cursor);
        }
        charge -= part;
    }
}

int32_t SculkSpreader::getTotalCharge() const {
    int32_t total = 0;
    for (const SculkChargeCursor &cursor: mCursors)
        total += cursor.mCharge;
    return total;
}

bool SculkSpreader::isReplaceable(const BlockState &state) const {
    if (replaceableBlocks().count(state.mName) != 0)
        return true;
    return mSettings.mWorldGeneration && worldGenerationExtras().count(state.mName) != 0;
}

// the last shuffled neighbour that is sculk or a vein, or the first vein holding onto something sculk can take over
bool SculkSpreader::_findMovement(SculkWorld &world, const Vector3i &from, std::mt19937 &random, Vector3i &to) const {
    std::array<Vector3i, 18> offsets = NON_CORNER_NEIGHBOURS;
    for (size_t index = 1; index < offsets.size(); ++index) {
        const size_t swap = (size_t) (random() % (uint32_t) (index + 1));
        if (swap != index)
            std::swap(offsets[index], offsets[swap]);
    }

    bool found = false;
    for (const Vector3i &offset: offsets) {
        const Vector3i next = add(from, offset);
        const BlockState &state = world.getBlock(next);
        const SculkBehaviours::Kind kind = SculkBehaviours::kindOf(state);
        if (kind == SculkBehaviours::Kind::Default || !isMovementUnobstructed(world, from, next))
            continue;
        to = next;
        found = true;
        if (kind == SculkBehaviours::Kind::Sculk)
            continue;
        for (int32_t face: {1, 0, 2, 3, 4, 5}) {
            if (SculkBehaviours::hasFace(state, face) && isReplaceable(world.getBlock(add(next, faceStep(face)))))
                return true;
        }
    }
    return found;
}

void SculkSpreader::_updateCursor(SculkWorld &world, const Vector3i &origin, std::mt19937 &random, bool convertBlocks,
                                  SculkChargeCursor &cursor) {
    if (cursor.mUpdateDelay > 0) {
        --cursor.mUpdateDelay;
        return;
    }

    SculkBehaviours::Kind kind = SculkBehaviours::kindOf(world.getBlock(cursor.mPosition));
    if (convertBlocks && SculkBehaviours::attemptSpreadVein(kind, world, cursor.mPosition, cursor.mFacing)) {
        if (SculkBehaviours::canChangeBlockStateOnSpread(kind))
            kind = SculkBehaviours::kindOf(world.getBlock(cursor.mPosition));
        world.playSound(SOUND_SPREAD, cursor.mPosition);
    }

    cursor.mCharge = SculkBehaviours::attemptUseCharge(kind, world, origin, cursor, random, *this, convertBlocks);
    if (cursor.mCharge <= 0) {
        SculkBehaviours::onDischarged(kind, world, cursor.mPosition);
        return;
    }

    Vector3i next;
    if (_findMovement(world, cursor.mPosition, random, next)) {
        SculkBehaviours::onDischarged(kind, world, cursor.mPosition);
        cursor.mPosition = next;
        if (mSettings.mWorldGeneration) {
            const int64_t dx = (int64_t) next.x - origin.x;
            const int64_t dy = (int64_t) next.y - origin.y;
            const int64_t dz = (int64_t) next.z - origin.z;
            if (dx * dx + dy * dy + dz * dz >= WORLD_GENERATION_REACH_SQUARED) {
                cursor.mCharge = 0;
                return;
            }
        }
    }

    const BlockState &state = world.getBlock(cursor.mPosition);
    const SculkBehaviours::Kind here = SculkBehaviours::kindOf(state);
    if (here != SculkBehaviours::Kind::Default) {
        cursor.mFacing = SculkBehaviours::facingsOf(here, state, cursor.mFacing);
        kind = here;
    }
    cursor.mUpdateDelay = 1;
    cursor.mDecayDelay = SculkBehaviours::updateDecayDelay(kind, cursor.mDecayDelay);
}

// each cursor steps, cursors meeting on a block merge, and the client sees the charge; world generation merges
// nothing and shows nothing
void SculkSpreader::updateCursors(ServerNetworkHandler *owner, Level &level, const Vector3i &origin,
                                  std::mt19937 &random, bool convertBlocks) {
    LevelSculkWorld world(owner, level);
    updateCursors(world, origin, random, convertBlocks);
}

void SculkSpreader::updateCursors(SculkWorld &world, const Vector3i &origin, std::mt19937 &random, bool convertBlocks) {
    if (mCursors.empty())
        return;

    if (mSettings.mWorldGeneration) {
        size_t kept = 0;
        for (SculkChargeCursor &cursor: mCursors) {
            if (cursor.mCharge <= 0)
                continue;
            if (world.isTicking(cursor.mPosition))
                _updateCursor(world, origin, random, convertBlocks, cursor);
            if (cursor.mCharge > 0)
                mCursors[kept++] = cursor;
        }
        mCursors.resize(kept);
        return;
    }

    std::vector<SculkChargeCursor> next;
    next.reserve(mCursors.size());
    // the charge per block for the client, and the cursor a merge joins
    std::map<std::tuple<int32_t, int32_t, int32_t>, int32_t> chargeAt;
    std::map<std::tuple<int32_t, int32_t, int32_t>, size_t> cursorAt;

    for (SculkChargeCursor cursor: mCursors) {
        if (cursor.mCharge <= 0) {
            world.addParticle(GenericLevelEventParticle(EVENT_SCULK_CHARGE_POP, centerOf(cursor.mPosition),
                                                        chargeEvent(cursor.mPosition)));
            continue;
        }

        if (world.isTicking(cursor.mPosition))
            _updateCursor(world, origin, random, convertBlocks, cursor);
        if (cursor.mCharge <= 0) {
            world.addParticle(GenericLevelEventParticle(EVENT_SCULK_CHARGE_POP, centerOf(cursor.mPosition),
                                                        chargeEvent(cursor.mPosition)));
            continue;
        }

        const auto at = std::make_tuple(cursor.mPosition.x, cursor.mPosition.y, cursor.mPosition.z);
        chargeAt[at] += cursor.mCharge;
        const auto existing = cursorAt.find(at);
        if (existing != cursorAt.end() && !mSettings.mWorldGeneration) {
            SculkChargeCursor &joined = next[existing->second];
            if (joined.mCharge + cursor.mCharge <= MAX_CHARGE) {
                joined.mCharge += cursor.mCharge;
                joined.mUpdateDelay = std::min(joined.mUpdateDelay, cursor.mUpdateDelay);
                continue;
            }
        }
        next.push_back(cursor);
        if (existing == cursorAt.end() || cursor.mCharge < next[existing->second].mCharge)
            cursorAt[at] = next.size() - 1;
    }
    mCursors = std::move(next);

    for (const auto &entry: chargeAt) {
        const Vector3i position(std::get<0>(entry.first), std::get<1>(entry.first), std::get<2>(entry.first));
        Tag data = chargeEvent(position);
        data.putShort("charge", (int16_t) entry.second);
        data.putShort("facing", (int16_t) mCursors[cursorAt[entry.first]].mFacing);
        world.addParticle(GenericLevelEventParticle(EVENT_SCULK_CHARGE, centerOf(position), std::move(data)));
    }
}

void SculkSpreader::save(Tag &data) const {
    if (mCursors.empty())
        return;

    Tag cursors = Tag::ofList(Tag::Type::Compound);
    for (const SculkChargeCursor &cursor: mCursors)
        cursors.addToList(cursor.save());
    data.put(TAG_CURSORS, cursors);
}

void SculkSpreader::load(const Tag &data) {
    if (!mCursors.empty())
        return;

    const Tag *cursors = data.get(TAG_CURSORS);
    if (cursors == nullptr || !cursors->isList())
        return;

    for (const Tag &entry: cursors->getList()) {
        if (entry.isCompound() && mCursors.size() < MAX_CURSORS)
            mCursors.push_back(SculkChargeCursor::load(entry));
    }
}

const BlockState &LevelSculkWorld::getBlock(const Vector3i &position) {
    return mLevel.getChunk(position.x >> 4, position.z >> 4).getBlock(position.x & 15, position.y, position.z & 15);
}

void LevelSculkWorld::setBlock(const Vector3i &position, const BlockState &state) {
    mLevel.setBlock(position, state, true);
}

bool LevelSculkWorld::isWaterSource(const Vector3i &position) {
    const LiquidInfo liquid = mLevel.getLiquidInfo(position.x, position.y, position.z);
    return liquid.water && liquid.source;
}

void LevelSculkWorld::placeGrowth(const Vector3i &position, const BlockState &state) {
    mLevel.setBlock(position, state, true);
    mLevel.onBlockPlaced(position, state);
}

bool LevelSculkWorld::isTicking(const Vector3i &position) {
    return mLevel.isColumnActive(position.x >> 4, position.z >> 4);
}

void LevelSculkWorld::playSound(const char *sound, const Vector3i &position) {
    if (mOwner != nullptr)
        mOwner->playLevelSound(mLevel, sound, Vector3f((float) position.x, (float) position.y, (float) position.z));
}

void LevelSculkWorld::addParticle(const GenericLevelEventParticle &particle) {
    mLevel.addParticle(particle);
}
