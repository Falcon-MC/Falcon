#include "Block/Blocks/PlantGrowthBlocks.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Blocks/WaterBlock.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"

FALCON_REGISTER_BLOCK(CactusBlock, 311);
FALCON_REGISTER_BLOCK(ReedsBlock, 312);
FALCON_REGISTER_BLOCK(BambooSaplingBlock, 313);
FALCON_REGISTER_BLOCK(BambooBlock, 314);
FALCON_REGISTER_BLOCK(KelpBlock, 315);
FALCON_REGISTER_BLOCK(VineBlock, 316);
FALCON_REGISTER_BLOCK(CaveVinesBlock, 317);

namespace {
    const char *AGE = "age";
    const char *AGE_BIT = "age_bit";
    const char *LEAF_SIZE = "bamboo_leaf_size";
    const char *STALK_THICKNESS = "bamboo_stalk_thickness";
    const char *KELP_AGE = "kelp_age";
    const char *VINE_BITS = "vine_direction_bits";
    const char *PLANT_AGE = "growing_plant_age";

    const int32_t MAX_AGE = 15;
    const int32_t MAX_CACTUS_HEIGHT = 3;
    const int32_t MAX_REEDS_HEIGHT = 3;
    const int32_t MAX_BAMBOO_HEIGHT = 16;
    const int32_t BAMBOO_SLOWDOWN_HEIGHT = 11;
    const int32_t BAMBOO_MIN_LIGHT = 9;
    const int32_t MAX_KELP_AGE = 25;
    const int32_t KELP_GROWTH_PERCENT = 14;
    const int32_t MAX_PLANT_AGE = 25;
    const int32_t CAVE_VINES_GROWTH_CHANCE = 10;
    const int32_t CAVE_VINES_BERRY_PERCENT = 11;
    const int32_t VINE_SPREAD_LIMIT = 5;
    const int32_t VINE_SPREAD_RADIUS = 4;

    const int32_t FACE_DOWN = 0;
    const int32_t FACE_UP = 1;
    const int32_t FACE_NORTH = 2;
    const int32_t FACE_SOUTH = 3;
    const int32_t FACE_WEST = 4;
    const int32_t FACE_EAST = 5;

    const int32_t HORIZONTAL_FACES[] = {FACE_NORTH, FACE_SOUTH, FACE_WEST, FACE_EAST};

    const Vector3i FACE_OFFSETS[] = {
            Vector3i(0, -1, 0), Vector3i(0, 1, 0), Vector3i(0, 0, -1),
            Vector3i(0, 0, 1), Vector3i(-1, 0, 0), Vector3i(1, 0, 0)
    };

    Vector3i side(const Vector3i &position, int32_t face) {
        const Vector3i &offset = FACE_OFFSETS[face];
        return Vector3i(position.x + offset.x, position.y + offset.y, position.z + offset.z);
    }

    Vector3i above(const Vector3i &position) {
        return side(position, FACE_UP);
    }

    Vector3i below(const Vector3i &position) {
        return side(position, FACE_DOWN);
    }

    BlockState stateAt(Level &level, const Vector3i &position) {
        return level.getBlockState(position.x, position.y, position.z);
    }

    bool isAirAt(Level &level, const Vector3i &position) {
        return DecorationSupport::isAir(stateAt(level, position));
    }

    bool isSolidAt(Level &level, const Vector3i &position) {
        return level.isSolidAt(position.x, position.y, position.z);
    }

    bool isInRange(Level &level, const Vector3i &position) {
        return position.y >= level.getMinY() && position.y <= level.getMaxY();
    }

    bool isWaterSource(const BlockState &state) {
        return WaterBlock::matches(state.mName) && state.mStates.getInt("liquid_depth", 0) == 0;
    }

    int32_t rotateClockwise(int32_t face) {
        switch (face) {
            case FACE_NORTH:
                return FACE_EAST;
            case FACE_EAST:
                return FACE_SOUTH;
            case FACE_SOUTH:
                return FACE_WEST;
            default:
                return FACE_NORTH;
        }
    }

    int32_t rotateCounterClockwise(int32_t face) {
        switch (face) {
            case FACE_NORTH:
                return FACE_WEST;
            case FACE_WEST:
                return FACE_SOUTH;
            case FACE_SOUTH:
                return FACE_EAST;
            default:
                return FACE_NORTH;
        }
    }

    int32_t opposite(int32_t face) {
        return face ^ 1;
    }

    int32_t vineBit(int32_t face) {
        switch (face) {
            case FACE_WEST:
                return 0x02;
            case FACE_NORTH:
                return 0x04;
            case FACE_EAST:
                return 0x08;
            default:
                return 0x01;
        }
    }

    bool isHorizontal(int32_t face) {
        return face >= FACE_NORTH;
    }

    bool randomBoolean() {
        return RandomTickSystem::nextInt(2) == 0;
    }

    void resetAge(Level &level, const Vector3i &position, const BlockState &state) {
        level.setBlock(position, DecorationSupport::withState(state, AGE, 0), false);
    }

    bool isHorizontalNeighbourhoodClear(Level &level, const Vector3i &position) {
        for (int32_t face: HORIZONTAL_FACES) {
            if (!isAirAt(level, side(position, face)))
                return false;
        }
        return true;
    }
}

bool CactusBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:cactus";
}

void CactusBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state) const {
    (void) owner;

    if (matches(stateAt(level, below(position)).mName))
        return;

    const int32_t age = state.mStates.getInt(AGE, 0);
    if (age < MAX_AGE) {
        level.setBlock(position, DecorationSupport::withState(state, AGE, age + 1), false);
        return;
    }

    for (int32_t offset = 1; offset <= MAX_CACTUS_HEIGHT; ++offset) {
        const Vector3i target(position.x, position.y + offset, position.z);
        const BlockState targetState = stateAt(level, target);
        if (matches(targetState.mName))
            continue;

        if (!DecorationSupport::isAir(targetState) || !isInRange(level, target))
            return;

        const int32_t roll = RandomTickSystem::nextInt(101);
        const bool flower = (offset < 2 && roll < 10) || (offset == MAX_CACTUS_HEIGHT && roll <= 25);
        if ((flower && !isHorizontalNeighbourhoodClear(level, target)) || (!flower && offset == MAX_CACTUS_HEIGHT)) {
            resetAge(level, position, state);
            return;
        }

        const BlockState grown = flower ? VanillaBlocks::CACTUS_FLOWER().toBlockState()
                                        : VanillaBlocks::CACTUS().toBlockState();
        level.setBlock(target, grown, true);
        break;
    }

    resetAge(level, position, state);
}

bool ReedsBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:reeds";
}

void ReedsBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state) const {
    (void) owner;

    const int32_t age = state.mStates.getInt(AGE, 0);
    if (age < MAX_AGE) {
        level.setBlock(position, DecorationSupport::withState(state, AGE, age + 1), false);
        return;
    }

    const Vector3i top = above(position);
    if (!isInRange(level, top) || !isAirAt(level, top))
        return;

    int32_t height = 0;
    Vector3i current = position;
    while (height < MAX_REEDS_HEIGHT && matches(stateAt(level, current).mName)) {
        ++height;
        current = below(current);
    }

    if (height >= MAX_REEDS_HEIGHT)
        return;

    level.setBlock(top, VanillaBlocks::REEDS().toBlockState(), true);
    resetAge(level, position, state);
}

bool BambooSaplingBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:bamboo_sapling";
}

void BambooSaplingBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state) const {
    (void) owner;

    const Vector3i top = above(position);
    if (state.mStates.getInt(AGE_BIT, 0) != 0 || !isInRange(level, top) || !isAirAt(level, top))
        return;

    if (RandomTickSystem::getFullLight(level, top) < BAMBOO_MIN_LIGHT || RandomTickSystem::nextInt(3) != 0)
        return;

    level.setBlock(top, DecorationSupport::withState(VanillaBlocks::BAMBOO().toBlockState(), LEAF_SIZE,
                                                     "small_leaves"), true);
}

void BambooSaplingBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                            const BlockState &state) const {
    PlantBlock::onNeighbourChanged(owner, level, position, state);

    const BlockState top = stateAt(level, above(position));
    if (!BambooBlock::matches(top.mName) || !matches(stateAt(level, position).mName))
        return;

    const std::string thickness = top.mStates.getString(STALK_THICKNESS, "thin");
    level.setBlock(position, DecorationSupport::withState(VanillaBlocks::BAMBOO().toBlockState(), STALK_THICKNESS,
                                                          thickness.c_str()), true);
}

bool BambooBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:bamboo";
}

void BambooBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state) const {
    (void) owner;

    const Vector3i top = above(position);
    if (state.mStates.getInt(AGE_BIT, 0) != 0 || !isInRange(level, top) || !isAirAt(level, top))
        return;

    if (RandomTickSystem::getFullLight(level, top) < BAMBOO_MIN_LIGHT)
        return;

    const int32_t height = countStalkBelow(level, position) + 1;
    if (height < MAX_BAMBOO_HEIGHT && RandomTickSystem::nextInt(3) == 0)
        grow(level, position, state, height);
}

int32_t BambooBlock::countStalkBelow(Level &level, const Vector3i &position) {
    int32_t count = 0;
    Vector3i current = below(position);
    while (count < MAX_BAMBOO_HEIGHT && matches(stateAt(level, current).mName)) {
        ++count;
        current = below(current);
    }
    return count;
}

void BambooBlock::grow(Level &level, const Vector3i &position, const BlockState &state, int32_t height) {
    const Vector3i underPosition = below(position);
    const Vector3i underUnderPosition = below(underPosition);
    const BlockState under = stateAt(level, underPosition);
    const BlockState underUnder = stateAt(level, underUnderPosition);

    const char *leaves = "no_leaves";
    if (height >= 1) {
        if (!matches(under.mName) || under.mStates.getString(LEAF_SIZE, "no_leaves") == "no_leaves") {
            leaves = "small_leaves";
        } else {
            leaves = "large_leaves";
            if (matches(underUnder.mName)) {
                level.setBlock(underPosition, DecorationSupport::withState(under, LEAF_SIZE, "small_leaves"), true);
                level.setBlock(underUnderPosition, DecorationSupport::withState(underUnder, LEAF_SIZE, "no_leaves"),
                               true);
            }
        }
    }

    const bool thick = state.mStates.getString(STALK_THICKNESS, "thin") == "thick" || matches(underUnder.mName);
    const bool stopped = height == MAX_BAMBOO_HEIGHT - 1
                         || (height >= BAMBOO_SLOWDOWN_HEIGHT && RandomTickSystem::nextInt(4) == 0);

    BlockState grown = VanillaBlocks::BAMBOO().toBlockState();
    grown = DecorationSupport::withState(grown, STALK_THICKNESS, thick ? "thick" : "thin");
    grown = DecorationSupport::withState(grown, LEAF_SIZE, leaves);
    grown = DecorationSupport::withState(grown, AGE_BIT, stopped ? 1 : 0);
    level.setBlock(above(position), grown, true);
}

bool KelpBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:kelp";
}

void KelpBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const BlockState &state) const {
    (void) owner;

    const int32_t age = state.mStates.getInt(KELP_AGE, 0);
    if (age >= MAX_KELP_AGE || RandomTickSystem::nextInt(100) >= KELP_GROWTH_PERCENT)
        return;

    const Vector3i top = above(position);
    if (!isInRange(level, top) || !isWaterSource(stateAt(level, top)))
        return;

    level.setBlockStateAtLayer(top.x, top.y, top.z, 1, WaterBlock::source());
    level.setBlock(top, DecorationSupport::withState(state, KELP_AGE, age + 1), true);
}

bool VineBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:vine";
}

void VineBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const BlockState &state) const {
    (void) owner;

    if (RandomTickSystem::nextInt(4) != 0)
        return;

    const int32_t face = RandomTickSystem::nextInt(6);
    const Vector3i target = side(position, face);
    int32_t bits = state.mStates.getInt(VINE_BITS, 0);

    if (face == FACE_UP && isInRange(level, target) && isAirAt(level, target)) {
        if (!canSpread(level, position))
            return;

        for (int32_t horizontal: HORIZONTAL_FACES) {
            if (randomBoolean() || !isSolidAt(level, above(side(position, horizontal))))
                bits &= ~vineBit(horizontal);
        }
        putVineOnHorizontalFace(level, target, bits);
        return;
    }

    if (isHorizontal(face) && (bits & vineBit(face)) != vineBit(face)) {
        if (!canSpread(level, position))
            return;

        const BlockState targetState = stateAt(level, target);
        if (!DecorationSupport::isAir(targetState)) {
            if (!DecorationSupport::isTransparent(targetState))
                putVine(level, position, bits | vineBit(face));
            return;
        }

        const int32_t clockwise = rotateClockwise(face);
        const int32_t counterClockwise = rotateCounterClockwise(face);
        const Vector3i clockwiseTarget = side(target, clockwise);
        const Vector3i counterClockwiseTarget = side(target, counterClockwise);
        const bool onClockwise = (bits & vineBit(clockwise)) == vineBit(clockwise);
        const bool onCounterClockwise = (bits & vineBit(counterClockwise)) == vineBit(counterClockwise);

        if (onClockwise && isSolidAt(level, clockwiseTarget))
            putVine(level, target, vineBit(clockwise));
        else if (onCounterClockwise && isSolidAt(level, counterClockwiseTarget))
            putVine(level, target, vineBit(counterClockwise));
        else if (onClockwise && isAirAt(level, clockwiseTarget) && isSolidAt(level, side(position, clockwise)))
            putVine(level, clockwiseTarget, vineBit(opposite(face)));
        else if (onCounterClockwise && isAirAt(level, counterClockwiseTarget)
                 && isSolidAt(level, side(position, counterClockwise)))
            putVine(level, counterClockwiseTarget, vineBit(opposite(face)));
        else if (isSolidAt(level, above(target)))
            putVine(level, target, 0);
        return;
    }

    const Vector3i under = below(position);
    if (!isInRange(level, under))
        return;

    const BlockState underState = stateAt(level, under);
    const bool underIsVine = matches(underState.mName);
    if (!underIsVine && !DecorationSupport::isAir(underState))
        return;

    for (int32_t horizontal: HORIZONTAL_FACES) {
        if (randomBoolean())
            bits &= ~vineBit(horizontal);
    }

    const int32_t underBits = underIsVine ? underState.mStates.getInt(VINE_BITS, 0) : 0;
    putVineOnHorizontalFace(level, under, underBits | bits);
}

bool VineBlock::canSpread(Level &level, const Vector3i &position) {
    int32_t count = 0;
    for (int32_t x = position.x - VINE_SPREAD_RADIUS; x <= position.x + VINE_SPREAD_RADIUS; ++x) {
        for (int32_t z = position.z - VINE_SPREAD_RADIUS; z <= position.z + VINE_SPREAD_RADIUS; ++z) {
            for (int32_t y = position.y - 1; y <= position.y + 1; ++y) {
                const BlockState *state = level.peekBlockPtr(x, y, z);
                if (state != nullptr && matches(state->mName) && ++count >= VINE_SPREAD_LIMIT)
                    return false;
            }
        }
    }
    return true;
}

void VineBlock::putVine(Level &level, const Vector3i &position, int32_t bits) {
    const BlockState existing = stateAt(level, position);
    if (matches(existing.mName) && existing.mStates.getInt(VINE_BITS, 0) == bits)
        return;

    level.setBlock(position, DecorationSupport::withState(VanillaBlocks::VINE().toBlockState(), VINE_BITS, bits),
                   true);
}

void VineBlock::putVineOnHorizontalFace(Level &level, const Vector3i &position, int32_t bits) {
    for (int32_t horizontal: HORIZONTAL_FACES) {
        if ((bits & vineBit(horizontal)) == vineBit(horizontal)) {
            putVine(level, position, bits);
            return;
        }
    }
}

bool CaveVinesBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:cave_vines" || identifier == "minecraft:cave_vines_head_with_berries"
           || identifier == "minecraft:cave_vines_body_with_berries";
}

void CaveVinesBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state) const {
    (void) owner;

    const Vector3i tip = below(position);
    const int32_t age = state.mStates.getInt(PLANT_AGE, 0);
    if (age >= MAX_PLANT_AGE || !isInRange(level, tip) || !isAirAt(level, tip))
        return;

    if (RandomTickSystem::nextInt(CAVE_VINES_GROWTH_CHANCE) != 0)
        return;

    const bool berries = RandomTickSystem::nextInt(100) < CAVE_VINES_BERRY_PERCENT;
    const BlockState grown = berries ? VanillaBlocks::CAVE_VINES_HEAD_WITH_BERRIES().toBlockState()
                                     : VanillaBlocks::CAVE_VINES().toBlockState();
    level.setBlock(tip, DecorationSupport::withState(grown, PLANT_AGE, age + 1), true);

    if (state.mName == "minecraft:cave_vines_head_with_berries") {
        const BlockState body = VanillaBlocks::CAVE_VINES_BODY_WITH_BERRIES().toBlockState();
        level.setBlock(position, DecorationSupport::withState(body, PLANT_AGE, age), false);
    }
}
