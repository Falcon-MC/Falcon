#include "Block/Blocks/VineBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/PlantGrowthHelpers.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"

FALCON_REGISTER_BLOCK(VineBlock, 316);

using namespace PlantGrowthHelpers;

namespace {
    const char *VINE_BITS = "vine_direction_bits";

    const int32_t VINE_SPREAD_LIMIT = 5;
    const int32_t VINE_SPREAD_RADIUS = 4;

    bool isSolidAt(Level &level, const Vector3i &position) {
        return level.isSolidAt(position.x, position.y, position.z);
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
