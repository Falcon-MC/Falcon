#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/PlacementHelpers.h"
#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/Sculk/SculkSpreader.h"
#include "World/Level/Sculk/SculkWorld.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
    const char *const STATE_FACES = "multi_face_direction_bits";
    const char *const STATE_CAN_SUMMON = "can_summon";
    const char *const SOUND_SPREAD = "block.sculk.spread";
    const char *const SOUND_SENSOR_PLACE = "block.sculk_sensor.place";
    const char *const SOUND_SHRIEKER_PLACE = "block.sculk_shrieker.place";
    // one growth in this many is a shrieker
    const int32_t SHRIEKER_CHANCE = 11;
    // no more than this many sensors and shriekers around a growth
    const int32_t MAX_NEARBY_GROWTHS = 3;
    const int32_t GROWTH_HORIZONTAL_RANGE = 4;
    // the decay penalty grows from the no growth radius out to 24 blocks
    const float PENALTY_START = 4.0f;
    const float PENALTY_SPAN_SQUARED = 400.0f;
    // the order faces are tried in when spreading veins over a block
    const std::array<int32_t, 6> SPREAD_ORDER = {1, 0, 2, 3, 4, 5};

    enum class SpreadType { SamePosition, SamePlane, WrapAround };

    int32_t nextInt(std::mt19937 &random, int32_t bound) {
        return bound <= 0 ? 0 : (int32_t) (random() % (uint32_t) bound);
    }

    Vector3i add(const Vector3i &a, const Vector3i &b) {
        return Vector3i(a.x + b.x, a.y + b.y, a.z + b.z);
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

    int32_t opposite(int32_t face) {
        return face ^ 1;
    }

    bool isAir(const BlockState &state) {
        return state.mName == BlockNames::AIR;
    }

    BlockState veinWith(int32_t bits) {
        static const BlockState vein = VanillaBlocks::SCULK_VEIN().toBlockState();
        Tag states = vein.mStates;
        states.putInt(STATE_FACES, bits);
        return BlockState(vein.mName, states);
    }

    const BlockState &sculkState() {
        static const BlockState sculk = VanillaBlocks::SCULK().toBlockState();
        return sculk;
    }

    // a vein can take this block's place: a water source, or anything replaceable that is not lava or fire
    bool veinCanReplace(SculkWorld &world, const Vector3i &position, const BlockState &state) {
        if (state.mName == BlockNames::WATER || state.mName == BlockNames::FLOWING_WATER)
            return world.isWaterSource(position);
        return BlockSupport::isReplaceable(state) && state.mName != BlockNames::LAVA
               && state.mName != BlockNames::FLOWING_LAVA && state.mName.find("fire") == std::string::npos;
    }

    // the vein config of the multiface spreader
    bool canSpreadFrom(const BlockState &state, int32_t face) {
        return state.mName != BlockNames::SCULK_VEIN || SculkBehaviours::hasFace(state, face);
    }

    bool isOtherBlockValidAsSource(const BlockState &state) {
        return state.mName != BlockNames::SCULK_VEIN;
    }

    bool canSpreadInto(SculkWorld &world, const Vector3i &position, int32_t face) {
        const BlockState support = world.getBlock(add(position, faceStep(face)));
        if (support.mName == BlockNames::SCULK_CATALYST || support.mName == BlockNames::MOVING_BLOCK
            || support.mName == BlockNames::SCULK)
            return false;
        const BlockState state = world.getBlock(position);
        if (veinCanReplace(world, position, state))
            return true;
        return state.mName == BlockNames::SCULK_VEIN || isAir(state) || world.isWaterSource(position);
    }

    // the state with face added, or the same state when the face is there already or has nothing to hold
    BlockState placementWith(SculkWorld &world, const BlockState &existing, const Vector3i &position, int32_t face) {
        const bool vein = existing.mName == BlockNames::SCULK_VEIN;
        if (vein && SculkBehaviours::hasFace(existing, face))
            return existing;
        if (!PlacementHelpers::hasSolidFace(world.getBlock(add(position, faceStep(face))), opposite(face)))
            return existing;
        const int32_t bits = vein ? existing.mStates.getInt(STATE_FACES, 0) : 0;
        return veinWith(bits | SculkBehaviours::faceBit(face));
    }

    bool differs(const BlockState &a, const BlockState &b) {
        return a.mName != b.mName || !(a.mStates == b.mStates);
    }

    // MultifaceSpreader::getSpreadFromFaceTowardDirection
    bool spreadTarget(SculkWorld &world, const BlockState &state, const Vector3i &position, int32_t from, int32_t to,
                      bool samePositionOnly, Vector3i &target, int32_t &targetFace) {
        if (!canSpreadFrom(state, from) || to == from || to == opposite(from))
            return false;
        if (!isOtherBlockValidAsSource(state)
            && (!SculkBehaviours::hasFace(state, from) || SculkBehaviours::hasFace(state, to)))
            return false;

        const std::array<SpreadType, 3> types = {SpreadType::SamePosition, SpreadType::SamePlane,
                                                 SpreadType::WrapAround};
        const size_t count = samePositionOnly ? 1 : types.size();
        for (size_t index = 0; index < count; ++index) {
            Vector3i candidate = position;
            int32_t face = to;
            if (types[index] == SpreadType::SamePlane) {
                candidate = add(position, faceStep(to));
                face = from;
            } else if (types[index] == SpreadType::WrapAround) {
                candidate = add(add(position, faceStep(to)), faceStep(from));
                face = opposite(to);
            }
            if (!canSpreadInto(world, candidate, face))
                continue;
            const BlockState current = world.getBlock(candidate);
            if (!differs(placementWith(world, current, candidate, face), current))
                continue;
            target = candidate;
            targetFace = face;
            return true;
        }
        return false;
    }

    int32_t spreadFromFace(SculkWorld &world, const Vector3i &position, int32_t from, bool samePositionOnly) {
        int32_t placed = 0;
        for (int32_t to: SPREAD_ORDER) {
            const BlockState state = world.getBlock(position);
            Vector3i target;
            int32_t face = 0;
            if (!spreadTarget(world, state, position, from, to, samePositionOnly, target, face))
                continue;
            const BlockState current = world.getBlock(target);
            const BlockState placedState = placementWith(world, current, target, face);
            if (differs(placedState, current)) {
                world.setBlock(target, placedState);
                ++placed;
            }
        }
        return placed;
    }

    // a vein loses the faces it lays on sculk with; with none left the water it held stays, or air
    void dischargeVein(SculkWorld &world, const Vector3i &position) {
        for (int32_t face: SPREAD_ORDER) {
            const BlockState state = world.getBlock(position);
            if (state.mName != BlockNames::SCULK_VEIN || !SculkBehaviours::hasFace(state, face))
                continue;
            if (world.getBlock(add(position, faceStep(face))).mName != BlockNames::SCULK)
                continue;
            const int32_t bits = state.mStates.getInt(STATE_FACES, 0) & ~SculkBehaviours::faceBit(face);
            if (bits != 0)
                world.setBlock(position, veinWith(bits));
            else if (world.isWaterSource(position))
                world.setBlock(position, VanillaBlocks::WATER().toBlockState());
            else
                world.setBlock(position, BlockState(BlockNames::AIR));
        }
    }

    // room for a sensor or a shrieker above the sculk, with few of them around
    bool canPlaceGrowth(SculkWorld &world, const Vector3i &position) {
        const Vector3i above(position.x, position.y + 1, position.z);
        const BlockState state = world.getBlock(above);
        if (!isAir(state) && state.mName != BlockNames::WATER)
            return false;
        int32_t growths = 0;
        for (int32_t x = above.x - GROWTH_HORIZONTAL_RANGE; x <= above.x + GROWTH_HORIZONTAL_RANGE; ++x) {
            for (int32_t y = above.y - 1; y <= above.y + 1; ++y) {
                for (int32_t z = above.z - GROWTH_HORIZONTAL_RANGE; z <= above.z + GROWTH_HORIZONTAL_RANGE; ++z) {
                    const std::string name = world.getBlock(Vector3i(x, y, z)).mName;
                    if (name == BlockNames::SCULK_SENSOR || name == BlockNames::SCULK_SHRIEKER)
                        ++growths;
                }
            }
        }
        return growths < MAX_NEARBY_GROWTHS;
    }

    int32_t sculkUseCharge(SculkWorld &world, const Vector3i &origin, const SculkChargeCursor &cursor,
                           std::mt19937 &random, const SculkSpreader &spreader) {
        const SculkSpreader::Settings &settings = spreader.getSettings();
        const int32_t charge = cursor.mCharge;
        if (nextInt(random, settings.mChargeDecayRate) != 0)
            return charge;

        const Vector3i &position = cursor.mPosition;
        const float dx = (float) position.x - (float) origin.x;
        const float dy = (float) position.y - (float) origin.y;
        const float dz = (float) position.z - (float) origin.z;
        const float distanceSquared = dx * dx + dy * dy + dz * dz;
        const float radiusSquared = (float) settings.mNoGrowthRadius * (float) settings.mNoGrowthRadius;
        if (radiusSquared <= distanceSquared && canPlaceGrowth(world, position)) {
            if (nextInt(random, settings.mGrowthSpawnCost) < charge) {
                const Vector3i above(position.x, position.y + 1, position.z);
                const bool shrieker = nextInt(random, SHRIEKER_CHANCE) == 0;
                if (shrieker) {
                    const BlockState base = VanillaBlocks::SCULK_SHRIEKER().toBlockState();
                    Tag states = base.mStates;
                    states.putByte(STATE_CAN_SUMMON, settings.mWorldGeneration ? 1 : 0);
                    world.placeGrowth(above, BlockState(base.mName, states));
                } else {
                    world.placeGrowth(above, VanillaBlocks::SCULK_SENSOR().toBlockState());
                }
                world.playSound(shrieker ? SOUND_SHRIEKER_PLACE : SOUND_SENSOR_PLACE, position);
            }
            return std::max(0, charge - settings.mGrowthSpawnCost);
        }

        if (nextInt(random, settings.mAdditionalDecayRate) != 0)
            return charge;
        int32_t penalty = 1;
        if (radiusSquared <= distanceSquared) {
            const int64_t ix = (int64_t) position.x - origin.x;
            const int64_t iy = (int64_t) position.y - origin.y;
            const int64_t iz = (int64_t) position.z - origin.z;
            const float squared = (float) (ix * ix + iy * iy + iz * iz);
            if ((float) (settings.mNoGrowthRadius * settings.mNoGrowthRadius) < squared) {
                const float distance = std::sqrt(squared);
                const int32_t scaled = (int32_t) ((float) charge
                                                  * std::min((distance - PENALTY_START) * (distance - PENALTY_START)
                                                                     / PENALTY_SPAN_SQUARED,
                                                             1.0f)
                                                  * 0.5f);
                if (scaled >= 2)
                    penalty = scaled;
            }
        }
        return charge - penalty;
    }

    int32_t veinUseCharge(SculkWorld &world, const SculkChargeCursor &cursor, std::mt19937 &random,
                          const SculkSpreader &spreader, bool convertBlocks) {
        const int32_t charge = cursor.mCharge;
        const Vector3i &position = cursor.mPosition;
        if (convertBlocks) {
            // the game shuffles its faces from up, down, north, south, west, east
            std::array<int32_t, 6> faces = SPREAD_ORDER;
            for (size_t index = 1; index < faces.size(); ++index) {
                const size_t swap = (size_t) (random() % (uint32_t) (index + 1));
                if (swap != index)
                    std::swap(faces[index], faces[swap]);
            }
            const BlockState state = world.getBlock(position);
            for (int32_t face: faces) {
                if (!SculkBehaviours::hasFace(state, face))
                    continue;
                const Vector3i sculk = add(position, faceStep(face));
                if (!spreader.isReplaceable(world.getBlock(sculk)))
                    continue;
                world.setBlock(sculk, sculkState());
                world.playSound(SOUND_SPREAD, position);
                for (int32_t around: SPREAD_ORDER) {
                    if (around != opposite(face))
                        dischargeVein(world, add(sculk, faceStep(around)));
                }
                SculkBehaviours::spreadVeinsAll(world, sculk, false);
                return charge - 1;
            }
        }
        return nextInt(random, spreader.getSettings().mChargeDecayRate) == 0
                       ? (int32_t) std::floor((float) charge * 0.5f)
                       : charge;
    }
}

namespace SculkBehaviours {
    Kind kindOf(const BlockState &state) {
        if (state.mName == BlockNames::SCULK)
            return Kind::Sculk;
        if (state.mName == BlockNames::SCULK_VEIN)
            return Kind::Vein;
        return Kind::Default;
    }

    int32_t faceBit(int32_t face) {
        static const std::array<int32_t, 6> BITS = {1, 2, 16, 4, 8, 32};
        return face >= 0 && face < 6 ? BITS[(size_t) face] : 0;
    }

    bool hasFace(const BlockState &state, int32_t face) {
        return state.mName == BlockNames::SCULK_VEIN && (state.mStates.getInt(STATE_FACES, 0) & faceBit(face)) != 0;
    }

    int32_t updateDecayDelay(Kind kind, int32_t decayDelay) {
        if (kind != Kind::Default)
            return 1;
        return std::max(decayDelay, 1) - 1;
    }

    int32_t facingsOf(Kind kind, const BlockState &state, int32_t facings) {
        if (kind == Kind::Sculk)
            return SculkChargeCursor::SCULK_FACES;
        if (kind == Kind::Vein)
            return state.mStates.getInt(STATE_FACES, 0);
        return facings;
    }

    bool canChangeBlockStateOnSpread(Kind kind) {
        return kind != Kind::Sculk;
    }

    int32_t spreadVeinsAll(Level &level, const Vector3i &position, bool samePositionOnly) {
        LevelSculkWorld world(nullptr, level);
        return spreadVeinsAll(world, position, samePositionOnly);
    }

    int32_t spreadVeinsAll(SculkWorld &world, const Vector3i &position, bool samePositionOnly) {
        int32_t placed = 0;
        for (int32_t from: SPREAD_ORDER)
            placed += spreadFromFace(world, position, from, samePositionOnly);
        return placed;
    }

    bool attemptSpreadVein(Kind kind, SculkWorld &world, const Vector3i &position, int32_t facings) {
        if (kind != Kind::Default || facings == SculkChargeCursor::SCULK_FACES)
            return spreadVeinsAll(world, position, false) > 0;
        if (facings == SculkChargeCursor::ALL_FACES)
            return spreadVeinsAll(world, position, true) > 0;
        if (!veinCanReplace(world, position, world.getBlock(position)))
            return false;
        world.setBlock(position, veinWith(facings));
        return true;
    }

    int32_t attemptUseCharge(Kind kind, SculkWorld &world, const Vector3i &origin, const SculkChargeCursor &cursor,
                             std::mt19937 &random, const SculkSpreader &spreader, bool convertBlocks) {
        switch (kind) {
            case Kind::Sculk:
                return sculkUseCharge(world, origin, cursor, random, spreader);
            case Kind::Vein:
                return veinUseCharge(world, cursor, random, spreader, convertBlocks);
            default:
                return cursor.mDecayDelay > 0 ? cursor.mCharge : 0;
        }
    }

    void onDischarged(Kind kind, SculkWorld &world, const Vector3i &position) {
        if (kind == Kind::Vein)
            dischargeVein(world, position);
    }
}
