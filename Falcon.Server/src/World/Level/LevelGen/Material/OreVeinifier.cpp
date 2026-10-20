#include "World/Level/LevelGen/Material/OreVeinifier.h"

#include "World/Level/Block/VanillaBlocks.h"

#include <cmath>
#include <utility>

namespace {

    const BlockState &copperOreState() {
        static const BlockState state = VanillaBlocks::COPPER_ORE().toBlockState();
        return state;
    }

    const BlockState &rawCopperBlockState() {
        static const BlockState state = VanillaBlocks::RAW_COPPER_BLOCK().toBlockState();
        return state;
    }

    const BlockState &graniteState() {
        static const BlockState state = VanillaBlocks::GRANITE().toBlockState();
        return state;
    }

    const BlockState &deepslateIronOreState() {
        static const BlockState state = VanillaBlocks::DEEPSLATE_IRON_ORE().toBlockState();
        return state;
    }

    const BlockState &rawIronBlockState() {
        static const BlockState state = VanillaBlocks::RAW_IRON_BLOCK().toBlockState();
        return state;
    }

    const BlockState &tuffState() {
        static const BlockState state = VanillaBlocks::TUFF().toBlockState();
        return state;
    }

    double javaClamp(double value, double min, double max) {
        return value < min ? min : (value < max ? value : max);
    }

}

OreVeinifier::VeinType OreVeinifier::_copperVein() {
    VeinType type;
    type.mOre = &copperOreState();
    type.mRawOreBlock = &rawCopperBlockState();
    type.mFiller = &graniteState();
    type.mMinY = 0;
    type.mMaxY = 50;
    return type;
}

OreVeinifier::VeinType OreVeinifier::_ironVein() {
    VeinType type;
    type.mOre = &deepslateIronOreState();
    type.mRawOreBlock = &rawIronBlockState();
    type.mFiller = &tuffState();
    type.mMinY = -60;
    type.mMaxY = -8;
    return type;
}

OreVeinifier::OreVeinifier(DensityFunctionPtr veinToggle, DensityFunctionPtr veinRidged, DensityFunctionPtr veinGap,
                           int64_t seed)
        : mVeinToggle(std::move(veinToggle)),
          mVeinRidged(std::move(veinRidged)),
          mVeinGap(std::move(veinGap)),
          mOreVeinSeed(seed),
          mRandom(seed) {
}

const BlockState *OreVeinifier::calculate(FunctionContext &context) {
    const int32_t y = context.blockY();
    if (y < MIN_VEIN_Y || y > MAX_VEIN_Y)
        return nullptr;

    const double oreVeininessNoiseValue = mVeinToggle->compute(context);
    const VeinType veinType = oreVeininessNoiseValue > 0.0 ? _copperVein() : _ironVein();
    const double veininessRidged = std::fabs(oreVeininessNoiseValue);
    const int32_t distanceFromTop = veinType.mMaxY - y;
    const int32_t distanceFromBottom = y - veinType.mMinY;
    if (distanceFromBottom < 0 || distanceFromTop < 0)
        return nullptr;

    const int32_t distanceFromEdge = distanceFromTop < distanceFromBottom ? distanceFromTop : distanceFromBottom;
    const double edgeRoundoff = _clampedMap((double) distanceFromEdge, 0.0, (double) EDGE_ROUNDOFF_BEGIN,
                                            -MAX_EDGE_ROUNDOFF, 0.0);
    if (veininessRidged + edgeRoundoff < (double) VEININESS_THRESHOLD)
        return nullptr;

    if (mVeinRidged->compute(context) >= 0.0)
        return nullptr;

    mRandom.setSeed(_mixSeed(mOreVeinSeed, context.blockX(), y, context.blockZ()));
    if (mRandom.nextFloat() > VEIN_SOLIDNESS)
        return nullptr;

    const double richness = _clampedMap(
            veininessRidged,
            (double) VEININESS_THRESHOLD,
            (double) MAX_RICHNESS_THRESHOLD,
            (double) MIN_RICHNESS,
            (double) MAX_RICHNESS
    );
    if (mRandom.nextFloat() < richness && mVeinGap->compute(context) > (double) SKIP_ORE_IF_GAP_NOISE_IS_BELOW)
        return mRandom.nextFloat() < CHANCE_OF_RAW_ORE_BLOCK ? veinType.mRawOreBlock : veinType.mOre;

    return veinType.mFiller;
}

double OreVeinifier::_clampedMap(double value, double inMin, double inMax, double outMin, double outMax) {
    double t = (value - inMin) / (inMax - inMin);
    t = javaClamp(t, 0.0, 1.0);
    return outMin + (outMax - outMin) * t;
}

int64_t OreVeinifier::_mixSeed(int64_t seed, int32_t x, int32_t y, int32_t z) {
    uint64_t mixed = (uint64_t) seed;
    mixed ^= (uint64_t) ((int64_t) x * 341873128712LL);
    mixed ^= (uint64_t) ((int64_t) y * 132897987541LL);
    mixed ^= (uint64_t) ((int64_t) z * 42317861LL);
    mixed ^= (mixed >> 33);
    mixed *= 0xff51afd7ed558ccdULL;
    mixed ^= (mixed >> 33);
    mixed *= 0xc4ceb9fe1a85ec53ULL;
    mixed ^= (mixed >> 33);
    return (int64_t) mixed;
}
