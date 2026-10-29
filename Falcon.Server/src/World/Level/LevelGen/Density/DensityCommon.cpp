#include "World/Level/LevelGen/Density/DensityCommon.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>

namespace {

    constexpr int32_t FLAT_CACHE_CHUNK_SIZE_BLOCKS = 16;
    constexpr int32_t CELL_SIZE_XZ = 4;
    constexpr int32_t CELL_SIZE_Y = 8;
    constexpr int32_t CELL_XZ_MASK = CELL_SIZE_XZ - 1;
    constexpr int32_t CELL_Y_MASK = CELL_SIZE_Y - 1;
    constexpr int32_t CELL_VALUE_COUNT = CELL_SIZE_XZ * CELL_SIZE_Y * CELL_SIZE_XZ;
    constexpr double INV_CELL_SIZE_XZ = 1.0 / CELL_SIZE_XZ;
    constexpr double INV_CELL_SIZE_Y = 1.0 / CELL_SIZE_Y;

    double clampValue(double value, double min, double max) {
        return value < min ? min : std::min(value, max);
    }

    double clampedMap(double value, double fromY, double toY, double fromValue, double toValue) {
        if (fromY == toY) {
            return value < fromY ? fromValue : toValue;
        }

        double t = clampValue((value - fromY) / (toY - fromY), 0.0, 1.0);
        return fromValue + t * (toValue - fromValue);
    }

    double applyMappedType(MappedType type, double input) {
        switch (type) {
            case MappedType::ABS:
                return std::abs(input);
            case MappedType::SQUARE:
                return input * input;
            case MappedType::CUBE:
                return input * input * input;
            case MappedType::HALF_NEGATIVE:
                return input > 0.0 ? input : input * 0.5;
            case MappedType::QUARTER_NEGATIVE:
                return input > 0.0 ? input : input * 0.25;
            case MappedType::INVERT:
                return input == 0.0 ? std::numeric_limits<double>::infinity() : 1.0 / input;
            case MappedType::SQUEEZE: {
                double clamped = clampValue(input, -1.0, 1.0);
                return clamped / 2.0 - clamped * clamped * clamped / 24.0;
            }
        }

        return input;
    }

    double getSpaghettiRarity2D(double value) {
        if (value < -0.75) {
            return 0.5;
        } else if (value < -0.5) {
            return 0.75;
        } else if (value < 0.5) {
            return 1.0;
        } else if (value < 0.75) {
            return 2.0;
        } else {
            return 3.0;
        }
    }

    double getSpaghettiRarity3D(double value) {
        if (value < -0.5) {
            return 0.75;
        } else if (value < 0.0) {
            return 1.0;
        } else if (value < 0.5) {
            return 1.5;
        } else {
            return 2.0;
        }
    }

    double applyRarityMapper(RarityValueMapper mapper, double value) {
        if (mapper == RarityValueMapper::TYPE1) {
            return getSpaghettiRarity3D(value);
        }

        return getSpaghettiRarity2D(value);
    }

    double maxRarityOf(RarityValueMapper mapper) {
        if (mapper == RarityValueMapper::TYPE1) {
            return 2.0;
        }

        return 3.0;
    }

    class ConstantFunction : public DensityFunction {
    public:
        explicit ConstantFunction(double value) : mValue(value) {}

        double compute(FunctionContext &context) override {
            (void) context;
            return mValue;
        }

        double minValue() const override {
            return mValue;
        }

        double maxValue() const override {
            return mValue;
        }

        double value() const {
            return mValue;
        }

    private:
        double mValue;
    };

    class ClampFunction : public DensityFunction {
    public:
        ClampFunction(DensityFunctionPtr input, double min, double max)
                : mInput(std::move(input)), mMin(min), mMax(max) {}

        double compute(FunctionContext &context) override {
            return clampValue(mInput->compute(context), mMin, mMax);
        }

        double minValue() const override {
            return mMin;
        }

        double maxValue() const override {
            return mMax;
        }

    private:
        DensityFunctionPtr mInput;
        double mMin;
        double mMax;
    };

    class NoiseFunction : public DensityFunction {
    public:
        NoiseFunction(NoiseHolder noise, double xzScale, double yScale)
                : mNoise(std::move(noise)), mXzScale(xzScale), mYScale(yScale) {}

        double compute(FunctionContext &context) override {
            return mNoise.getValue(context.blockX() * mXzScale, context.blockY() * mYScale,
                                   context.blockZ() * mXzScale);
        }

        double minValue() const override {
            return -maxValue();
        }

        double maxValue() const override {
            return mNoise.maxValue();
        }

    private:
        NoiseHolder mNoise;
        double mXzScale;
        double mYScale;
    };

    class RangeChoiceFunction : public DensityFunction {
    public:
        RangeChoiceFunction(DensityFunctionPtr input, double minInclusive, double maxExclusive,
                            DensityFunctionPtr whenInRange, DensityFunctionPtr whenOutOfRange)
                : mInput(std::move(input)), mMinInclusive(minInclusive), mMaxExclusive(maxExclusive),
                  mWhenInRange(std::move(whenInRange)), mWhenOutOfRange(std::move(whenOutOfRange)) {}

        double compute(FunctionContext &context) override {
            double inputValue = mInput->compute(context);
            return inputValue >= mMinInclusive && inputValue < mMaxExclusive
                   ? mWhenInRange->compute(context)
                   : mWhenOutOfRange->compute(context);
        }

        double minValue() const override {
            return std::min(mWhenInRange->minValue(), mWhenOutOfRange->minValue());
        }

        double maxValue() const override {
            return std::max(mWhenInRange->maxValue(), mWhenOutOfRange->maxValue());
        }

    private:
        DensityFunctionPtr mInput;
        double mMinInclusive;
        double mMaxExclusive;
        DensityFunctionPtr mWhenInRange;
        DensityFunctionPtr mWhenOutOfRange;
    };

    class WeirdScaledSamplerFunction : public DensityFunction {
    public:
        WeirdScaledSamplerFunction(DensityFunctionPtr input, NoiseHolder noise, RarityValueMapper rarityValueMapper)
                : mInput(std::move(input)), mNoise(std::move(noise)), mRarityValueMapper(rarityValueMapper) {}

        double compute(FunctionContext &context) override {
            double inputValue = mInput->compute(context);
            double rarity = applyRarityMapper(mRarityValueMapper, inputValue);
            return rarity * std::abs(mNoise.getValue(context.blockX() / rarity, context.blockY() / rarity,
                                                     context.blockZ() / rarity));
        }

        double minValue() const override {
            return 0.0;
        }

        double maxValue() const override {
            return maxRarityOf(mRarityValueMapper) * mNoise.maxValue();
        }

    private:
        DensityFunctionPtr mInput;
        NoiseHolder mNoise;
        RarityValueMapper mRarityValueMapper;
    };

    class YClampedGradientFunction : public DensityFunction {
    public:
        YClampedGradientFunction(int32_t fromY, int32_t toY, double fromValue, double toValue)
                : mFromY(fromY), mToY(toY), mFromValue(fromValue), mToValue(toValue) {}

        double compute(FunctionContext &context) override {
            return clampedMap(context.blockY(), mFromY, mToY, mFromValue, mToValue);
        }

        double minValue() const override {
            return std::min(mFromValue, mToValue);
        }

        double maxValue() const override {
            return std::max(mFromValue, mToValue);
        }

    private:
        int32_t mFromY;
        int32_t mToY;
        double mFromValue;
        double mToValue;
    };

    class BlendAlphaFunction : public DensityFunction {
    public:
        double compute(FunctionContext &context) override {
            (void) context;
            return 1.0;
        }

        double minValue() const override {
            return 1.0;
        }

        double maxValue() const override {
            return 1.0;
        }
    };

    class BlendOffsetFunction : public DensityFunction {
    public:
        double compute(FunctionContext &context) override {
            (void) context;
            return 0.0;
        }

        double minValue() const override {
            return 0.0;
        }

        double maxValue() const override {
            return 0.0;
        }
    };

    class BlockYFunction : public DensityFunction {
    public:
        double compute(FunctionContext &context) override {
            return context.blockY();
        }

        double minValue() const override {
            return -std::numeric_limits<double>::infinity();
        }

        double maxValue() const override {
            return std::numeric_limits<double>::infinity();
        }
    };

    class SplineFunction : public DensityFunction {
    public:
        explicit SplineFunction(CubicSplinePtr spline) : mSpline(std::move(spline)) {}

        double compute(FunctionContext &context) override {
            return mSpline->apply(context);
        }

        double minValue() const override {
            return mSpline->minValue();
        }

        double maxValue() const override {
            return mSpline->maxValue();
        }

    private:
        CubicSplinePtr mSpline;
    };

    class MappedFunction : public DensityFunction {
    public:
        MappedFunction(MappedType type, DensityFunctionPtr input, double minValue, double maxValue)
                : mType(type), mInput(std::move(input)), mMinValue(minValue), mMaxValue(maxValue) {}

        double compute(FunctionContext &context) override {
            return applyMappedType(mType, mInput->compute(context));
        }

        double minValue() const override {
            return mMinValue;
        }

        double maxValue() const override {
            return mMaxValue;
        }

    private:
        MappedType mType;
        DensityFunctionPtr mInput;
        double mMinValue;
        double mMaxValue;
    };

    class ShiftNoiseFunction : public DensityFunction {
    public:
        explicit ShiftNoiseFunction(NoiseHolder offsetNoise) : mOffsetNoise(std::move(offsetNoise)) {}

        double minValue() const override {
            return -maxValue();
        }

        double maxValue() const override {
            return mOffsetNoise.maxValue() * 4.0;
        }

    protected:
        double _computeShift(double x, double y, double z) {
            return mOffsetNoise.getValue(x * 0.25, y * 0.25, z * 0.25) * 4.0;
        }

    private:
        NoiseHolder mOffsetNoise;
    };

    class ShiftFunction : public ShiftNoiseFunction {
    public:
        explicit ShiftFunction(NoiseHolder offsetNoise) : ShiftNoiseFunction(std::move(offsetNoise)) {}

        double compute(FunctionContext &context) override {
            return _computeShift(context.blockX(), context.blockY(), context.blockZ());
        }
    };

    class ShiftAFunction : public ShiftNoiseFunction {
    public:
        explicit ShiftAFunction(NoiseHolder offsetNoise) : ShiftNoiseFunction(std::move(offsetNoise)) {}

        double compute(FunctionContext &context) override {
            return _computeShift(context.blockX(), 0.0, context.blockZ());
        }
    };

    class ShiftBFunction : public ShiftNoiseFunction {
    public:
        explicit ShiftBFunction(NoiseHolder offsetNoise) : ShiftNoiseFunction(std::move(offsetNoise)) {}

        double compute(FunctionContext &context) override {
            return _computeShift(context.blockZ(), context.blockX(), 0.0);
        }
    };

    class ShiftedNoiseFunction : public DensityFunction {
    public:
        ShiftedNoiseFunction(DensityFunctionPtr shiftX, DensityFunctionPtr shiftY, DensityFunctionPtr shiftZ,
                             double xzScale, double yScale, NoiseHolder noise)
                : mShiftX(std::move(shiftX)), mShiftY(std::move(shiftY)), mShiftZ(std::move(shiftZ)),
                  mXzScale(xzScale), mYScale(yScale), mNoise(std::move(noise)) {}

        double compute(FunctionContext &context) override {
            double x = context.blockX() * mXzScale + mShiftX->compute(context);
            double y = context.blockY() * mYScale + mShiftY->compute(context);
            double z = context.blockZ() * mXzScale + mShiftZ->compute(context);
            return mNoise.getValue(x, y, z);
        }

        double minValue() const override {
            return -maxValue();
        }

        double maxValue() const override {
            return mNoise.maxValue();
        }

    private:
        DensityFunctionPtr mShiftX;
        DensityFunctionPtr mShiftY;
        DensityFunctionPtr mShiftZ;
        double mXzScale;
        double mYScale;
        NoiseHolder mNoise;
    };

    class Ap2Function : public DensityFunction {
    public:
        Ap2Function(TwoArgumentType type, DensityFunctionPtr argument1, DensityFunctionPtr argument2, double minValue,
                    double maxValue)
                : mType(type), mArgument1(std::move(argument1)), mArgument2(std::move(argument2)), mMinValue(minValue),
                  mMaxValue(maxValue) {}

        double compute(FunctionContext &context) override {
            double v1 = mArgument1->compute(context);
            switch (mType) {
                case TwoArgumentType::ADD:
                    return v1 + mArgument2->compute(context);
                case TwoArgumentType::MUL:
                    return v1 == 0.0 ? 0.0 : v1 * mArgument2->compute(context);
                case TwoArgumentType::MIN:
                    return v1 < mArgument2->minValue() ? v1 : std::min(v1, mArgument2->compute(context));
                case TwoArgumentType::MAX:
                    return v1 > mArgument2->maxValue() ? v1 : std::max(v1, mArgument2->compute(context));
            }

            return v1;
        }

        double minValue() const override {
            return mMinValue;
        }

        double maxValue() const override {
            return mMaxValue;
        }

    private:
        TwoArgumentType mType;
        DensityFunctionPtr mArgument1;
        DensityFunctionPtr mArgument2;
        double mMinValue;
        double mMaxValue;
    };

    enum class MulOrAddType {
        MUL,
        ADD
    };

    class MulOrAddFunction : public DensityFunction {
    public:
        MulOrAddFunction(MulOrAddType specificType, DensityFunctionPtr input, double minValue, double maxValue,
                         double argument)
                : mSpecificType(specificType), mInput(std::move(input)), mMinValue(minValue), mMaxValue(maxValue),
                  mArgument(argument) {}

        double compute(FunctionContext &context) override {
            double inputValue = mInput->compute(context);
            if (mSpecificType == MulOrAddType::MUL) {
                return inputValue * mArgument;
            }

            return inputValue + mArgument;
        }

        double minValue() const override {
            return mMinValue;
        }

        double maxValue() const override {
            return mMaxValue;
        }

    private:
        MulOrAddType mSpecificType;
        DensityFunctionPtr mInput;
        double mMinValue;
        double mMaxValue;
        double mArgument;
    };

    class MarkerFunction : public DensityFunction {
    public:
        explicit MarkerFunction(DensityFunctionPtr wrapped) : mWrapped(std::move(wrapped)) {}

        double minValue() const override {
            return mWrapped->minValue();
        }

        double maxValue() const override {
            return mWrapped->maxValue();
        }

    protected:
        DensityFunctionPtr mWrapped;
    };

    class Cache2DState : public DensityMarkerState {
    public:
        int64_t mLastPos2d = 0;
        double mLastValue = 0.0;
    };

    class Cache2DMarker : public MarkerFunction {
    public:
        explicit Cache2DMarker(DensityFunctionPtr wrapped) : MarkerFunction(std::move(wrapped)) {}

        double compute(FunctionContext &context) override {
            DensityChunkCache *cache = context.densityChunkCache();
            if (cache == nullptr) {
                return mWrapped->compute(context);
            }

            Cache2DState *state = cache->getOrCreate<Cache2DState>(this);
            int32_t blockX = context.blockX();
            int32_t blockZ = context.blockZ();
            int64_t pos2d = (static_cast<int64_t>(blockX) << 32) ^ static_cast<int64_t>(
                    static_cast<uint32_t>(blockZ));
            if (state->mLastPos2d == pos2d) {
                return state->mLastValue;
            }

            state->mLastPos2d = pos2d;
            state->mLastValue = mWrapped->compute(context);
            return state->mLastValue;
        }
    };

    class Cache3DState : public DensityMarkerState {
    public:
        int32_t mBlockX = 0;
        int32_t mBlockY = 0;
        int32_t mBlockZ = 0;
        double mValue = 0.0;
    };

    class CacheOnceMarker : public MarkerFunction {
    public:
        explicit CacheOnceMarker(DensityFunctionPtr wrapped) : MarkerFunction(std::move(wrapped)) {}

        double compute(FunctionContext &context) override {
            DensityChunkCache *cache = context.densityChunkCache();
            if (cache == nullptr) {
                return mWrapped->compute(context);
            }

            Cache3DState *state = cache->getOrCreate<Cache3DState>(this);
            int32_t blockX = context.blockX();
            int32_t blockY = context.blockY();
            int32_t blockZ = context.blockZ();
            if (state->mBlockX == blockX && state->mBlockY == blockY && state->mBlockZ == blockZ) {
                return state->mValue;
            }

            state->mBlockX = blockX;
            state->mBlockY = blockY;
            state->mBlockZ = blockZ;
            state->mValue = mWrapped->compute(context);
            return state->mValue;
        }
    };

    struct FlatCacheCell {
        int32_t mFirstBlockX = 0;
        int32_t mFirstBlockZ = 0;
        double mValues[FLAT_CACHE_CHUNK_SIZE_BLOCKS * FLAT_CACHE_CHUNK_SIZE_BLOCKS] = {};
        uint64_t mFilledBits[(FLAT_CACHE_CHUNK_SIZE_BLOCKS * FLAT_CACHE_CHUNK_SIZE_BLOCKS + 63) / 64] = {};
    };

    class FlatCacheState : public DensityMarkerState {
    public:
        FlatCacheCell *getOrCreateCell(int32_t chunkX, int32_t chunkZ) {
            int64_t key = (static_cast<int64_t>(chunkX) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(chunkZ));

            if (key == mLastKey) {
                return mLastCell;
            }

            mLastKey = key;
            auto found = mCells.find(key);
            if (found != mCells.end()) {
                mLastCell = found->second.get();
                return mLastCell;
            }

            auto created = std::make_shared<FlatCacheCell>();
            created->mFirstBlockX = chunkX << 4;
            created->mFirstBlockZ = chunkZ << 4;
            mLastCell = created.get();
            mCells.emplace(key, std::move(created));
            return mLastCell;
        }

        MutableFunctionContext mContext;

    private:
        std::unordered_map<int64_t, std::shared_ptr<FlatCacheCell>> mCells;
        int64_t mLastKey = std::numeric_limits<int64_t>::max();
        FlatCacheCell *mLastCell = nullptr;
    };

    class FlatCacheMarker : public MarkerFunction {
    public:
        explicit FlatCacheMarker(DensityFunctionPtr wrapped) : MarkerFunction(std::move(wrapped)) {}

        double compute(FunctionContext &context) override {
            DensityChunkCache *cache = context.densityChunkCache();
            if (cache == nullptr) {
                return mWrapped->compute(context);
            }

            FlatCacheState *state = cache->getOrCreate<FlatCacheState>(this);
            int32_t blockX = context.blockX();
            int32_t blockZ = context.blockZ();
            int32_t chunkX = blockX >> 4;
            int32_t chunkZ = blockZ >> 4;
            FlatCacheCell *cell = state->getOrCreateCell(chunkX, chunkZ);
            int32_t localX = blockX - cell->mFirstBlockX;
            int32_t localZ = blockZ - cell->mFirstBlockZ;
            if (localX >= 0 && localX < FLAT_CACHE_CHUNK_SIZE_BLOCKS
                && localZ >= 0 && localZ < FLAT_CACHE_CHUNK_SIZE_BLOCKS) {
                int32_t index = localX + localZ * FLAT_CACHE_CHUNK_SIZE_BLOCKS;
                int32_t word = static_cast<int32_t>(static_cast<uint32_t>(index) >> 6);
                uint64_t mask = static_cast<uint64_t>(1) << (index & 63);
                if ((cell->mFilledBits[word] & mask) != 0) {
                    return cell->mValues[index];
                }

                double computed = mWrapped->compute(state->mContext.set(blockX, 0, blockZ));
                cell->mValues[index] = computed;
                cell->mFilledBits[word] |= mask;
                return computed;
            }

            return mWrapped->compute(context);
        }
    };

    class CellValuesState : public DensityMarkerState {
    public:
        double mValues[CELL_VALUE_COUNT] = {};
        MutableFunctionContext mContext;
        MutableChunkCacheContext mChunkCacheContext;
        int64_t mLastKey = std::numeric_limits<int64_t>::max();
    };

    int64_t packCellKey(int32_t cellX, int32_t cellY, int32_t cellZ) {
        return ((static_cast<int64_t>(cellX) & 0x1FFFFF) << 42)
               | ((static_cast<int64_t>(cellY) & 0x1FFFFF) << 21)
               | (static_cast<int64_t>(cellZ) & 0x1FFFFF);
    }

    template<typename ContextType>
    void fillAllInCell(CellValuesState &state, int32_t startX, int32_t startY, int32_t startZ,
                       const DensityFunctionPtr &wrapped, ContextType &context) {
        int32_t index = 0;
        for (int32_t localY = 0; localY < CELL_SIZE_Y; localY++) {
            int32_t y = startY + localY;
            for (int32_t localZ = 0; localZ < CELL_SIZE_XZ; localZ++) {
                int32_t z = startZ + localZ;
                for (int32_t localX = 0; localX < CELL_SIZE_XZ; localX++) {
                    state.mValues[index++] = wrapped->compute(context.set(startX + localX, y, z));
                }
            }
        }
    }

    void fillCellValues(CellValuesState &state, double d000, double d100, double d010, double d110, double d001,
                        double d101, double d011, double d111) {
        double c000 = d000;
        double c100 = d100 - d000;
        double c010 = d010 - d000;
        double c001 = d001 - d000;
        double c110 = d110 - d100 - d010 + d000;
        double c101 = d101 - d100 - d001 + d000;
        double c011 = d011 - d010 - d001 + d000;
        double c111 = d111 - d110 - d101 - d011 + d100 + d010 + d001 - d000;

        int32_t index = 0;
        for (int32_t localY = 0; localY < CELL_SIZE_Y; localY++) {
            double yAlpha = localY * INV_CELL_SIZE_Y;
            for (int32_t localZ = 0; localZ < CELL_SIZE_XZ; localZ++) {
                double zAlpha = localZ * INV_CELL_SIZE_XZ;
                double yz = yAlpha * zAlpha;
                double zTerm = zAlpha * (c001 + yAlpha * c011);
                for (int32_t localX = 0; localX < CELL_SIZE_XZ; localX++) {
                    double xAlpha = localX * INV_CELL_SIZE_XZ;
                    state.mValues[index++] = c000
                                             + xAlpha * (c100 + yAlpha * c110 + zAlpha * c101 + yz * c111)
                                             + yAlpha * c010
                                             + zTerm;
                }
            }
        }
    }

    template<typename ContextType>
    void fillInterpolatedCell(CellValuesState &state, int32_t cellX, int32_t cellY, int32_t cellZ,
                              const DensityFunctionPtr &wrapped, ContextType &context) {
        int32_t nextX = cellX + CELL_SIZE_XZ;
        int32_t nextY = cellY + CELL_SIZE_Y;
        int32_t nextZ = cellZ + CELL_SIZE_XZ;
        double d000 = wrapped->compute(context.set(cellX, cellY, cellZ));
        double d100 = wrapped->compute(context.set(nextX, cellY, cellZ));
        double d010 = wrapped->compute(context.set(cellX, nextY, cellZ));
        double d110 = wrapped->compute(context.set(nextX, nextY, cellZ));
        double d001 = wrapped->compute(context.set(cellX, cellY, nextZ));
        double d101 = wrapped->compute(context.set(nextX, cellY, nextZ));
        double d011 = wrapped->compute(context.set(cellX, nextY, nextZ));
        double d111 = wrapped->compute(context.set(nextX, nextY, nextZ));
        fillCellValues(state, d000, d100, d010, d110, d001, d101, d011, d111);
    }

    class CacheAllInCellMarker : public MarkerFunction {
    public:
        explicit CacheAllInCellMarker(DensityFunctionPtr wrapped) : MarkerFunction(std::move(wrapped)) {}

        double compute(FunctionContext &context) override {
            DensityChunkCache *cache = context.densityChunkCache();
            if (cache == nullptr) {
                return mWrapped->compute(context);
            }

            CellValuesState *state = cache->getOrCreate<CellValuesState>(this);
            int32_t blockX = context.blockX();
            int32_t blockY = context.blockY();
            int32_t blockZ = context.blockZ();
            int32_t cellX = blockX >> 2;
            int32_t cellY = blockY >> 3;
            int32_t cellZ = blockZ >> 2;

            int64_t key = packCellKey(cellX, cellY, cellZ);
            if (key != state->mLastKey) {
                fillAllInCell(*state, cellX << 2, cellY << 3, cellZ << 2, mWrapped,
                              state->mChunkCacheContext.withCache(cache));
                state->mLastKey = key;
            }

            int32_t index = ((blockY & CELL_Y_MASK) << 4)
                            | ((blockZ & CELL_XZ_MASK) << 2)
                            | (blockX & CELL_XZ_MASK);
            return state->mValues[index];
        }
    };

    class InterpolatedMarker : public MarkerFunction {
    public:
        explicit InterpolatedMarker(DensityFunctionPtr wrapped) : MarkerFunction(std::move(wrapped)) {}

        double compute(FunctionContext &context) override {
            DensityChunkCache *cache = context.densityChunkCache();
            CellValuesState localState;
            CellValuesState *state = cache != nullptr ? cache->getOrCreate<CellValuesState>(this) : &localState;

            int32_t blockX = context.blockX();
            int32_t blockY = context.blockY();
            int32_t blockZ = context.blockZ();
            int32_t cellX = (blockX >> 2) << 2;
            int32_t cellY = (blockY >> 3) << 3;
            int32_t cellZ = (blockZ >> 2) << 2;

            int64_t key = packCellKey(cellX, cellY, cellZ);
            if (key != state->mLastKey) {
                if (cache != nullptr) {
                    fillInterpolatedCell(*state, cellX, cellY, cellZ, mWrapped,
                                         state->mChunkCacheContext.withCache(cache));
                } else {
                    fillInterpolatedCell(*state, cellX, cellY, cellZ, mWrapped, state->mContext);
                }
                state->mLastKey = key;
            }

            int32_t index = ((blockY & CELL_Y_MASK) << 4)
                            | ((blockZ & CELL_XZ_MASK) << 2)
                            | (blockX & CELL_XZ_MASK);
            return state->mValues[index];
        }
    };

}

DensityFunctionPtr TwoArgumentSimpleFunction::create(TwoArgumentType type, DensityFunctionPtr argument1,
                                                     DensityFunctionPtr argument2) {
    double min1 = argument1->minValue();
    double min2 = argument2->minValue();
    double max1 = argument1->maxValue();
    double max2 = argument2->maxValue();

    double minValue = 0.0;
    switch (type) {
        case TwoArgumentType::ADD:
            minValue = min1 + min2;
            break;
        case TwoArgumentType::MUL:
            minValue = min1 > 0.0 && min2 > 0.0
                       ? min1 * min2
                       : (max1 < 0.0 && max2 < 0.0 ? max1 * max2 : std::min(min1 * max2, max1 * min2));
            break;
        case TwoArgumentType::MIN:
            minValue = std::min(min1, min2);
            break;
        case TwoArgumentType::MAX:
            minValue = std::max(min1, min2);
            break;
    }

    double maxValue = 0.0;
    switch (type) {
        case TwoArgumentType::ADD:
            maxValue = max1 + max2;
            break;
        case TwoArgumentType::MUL:
            maxValue = min1 > 0.0 && min2 > 0.0
                       ? max1 * max2
                       : (max1 < 0.0 && max2 < 0.0 ? min1 * min2 : std::max(min1 * min2, max1 * max2));
            break;
        case TwoArgumentType::MIN:
            maxValue = std::min(max1, max2);
            break;
        case TwoArgumentType::MAX:
            maxValue = std::max(max1, max2);
            break;
    }

    if (type == TwoArgumentType::ADD || type == TwoArgumentType::MUL) {
        auto constant1 = std::dynamic_pointer_cast<ConstantFunction>(argument1);
        if (constant1 != nullptr) {
            return std::make_shared<MulOrAddFunction>(
                    type == TwoArgumentType::ADD ? MulOrAddType::ADD : MulOrAddType::MUL,
                    std::move(argument2), minValue, maxValue, constant1->value());
        }
    }

    if (type == TwoArgumentType::ADD || type == TwoArgumentType::MUL) {
        auto constant2 = std::dynamic_pointer_cast<ConstantFunction>(argument2);
        if (constant2 != nullptr) {
            return std::make_shared<MulOrAddFunction>(
                    type == TwoArgumentType::ADD ? MulOrAddType::ADD : MulOrAddType::MUL,
                    std::move(argument1), minValue, maxValue, constant2->value());
        }
    }

    return std::make_shared<Ap2Function>(type, std::move(argument1), std::move(argument2), minValue, maxValue);
}

DensityFunctionPtr Mapped::create(MappedType type, DensityFunctionPtr input) {
    double min = input->minValue();
    double max = input->maxValue();
    double transformedMin = applyMappedType(type, min);
    double transformedMax = applyMappedType(type, max);

    if (type == MappedType::INVERT) {
        if (min < 0.0 && max > 0.0) {
            return std::make_shared<MappedFunction>(type, std::move(input),
                                                    -std::numeric_limits<double>::infinity(),
                                                    std::numeric_limits<double>::infinity());
        }

        return std::make_shared<MappedFunction>(type, std::move(input), std::min(transformedMin, transformedMax),
                                                std::max(transformedMin, transformedMax));
    }

    if (type == MappedType::ABS || type == MappedType::SQUARE) {
        return std::make_shared<MappedFunction>(type, std::move(input),
                                                std::max(0.0, std::min(transformedMin, transformedMax)),
                                                std::max(transformedMin, transformedMax));
    }

    return std::make_shared<MappedFunction>(type, std::move(input), std::min(transformedMin, transformedMax),
                                            std::max(transformedMin, transformedMax));
}

DensityFunctionPtr Marker::create(MarkerType type, DensityFunctionPtr wrapped) {
    switch (type) {
        case MarkerType::INTERPOLATED:
            return std::make_shared<InterpolatedMarker>(std::move(wrapped));
        case MarkerType::FLAT_CACHE:
            return std::make_shared<FlatCacheMarker>(std::move(wrapped));
        case MarkerType::CACHE_2D:
            return std::make_shared<Cache2DMarker>(std::move(wrapped));
        case MarkerType::CACHE_ONCE:
            return std::make_shared<CacheOnceMarker>(std::move(wrapped));
        case MarkerType::CACHE_ALL_IN_CELL:
            return std::make_shared<CacheAllInCellMarker>(std::move(wrapped));
    }

    return wrapped;
}

DensityFunctionPtr DensityCommon::interpolated(DensityFunctionPtr function) {
    return Marker::create(MarkerType::INTERPOLATED, std::move(function));
}

DensityFunctionPtr DensityCommon::flatCache(DensityFunctionPtr function) {
    return Marker::create(MarkerType::FLAT_CACHE, std::move(function));
}

DensityFunctionPtr DensityCommon::cache2d(DensityFunctionPtr function) {
    return Marker::create(MarkerType::CACHE_2D, std::move(function));
}

DensityFunctionPtr DensityCommon::cacheOnce(DensityFunctionPtr function) {
    return Marker::create(MarkerType::CACHE_ONCE, std::move(function));
}

DensityFunctionPtr DensityCommon::cacheAllInCell(DensityFunctionPtr function) {
    return Marker::create(MarkerType::CACHE_ALL_IN_CELL, std::move(function));
}

DensityFunctionPtr DensityCommon::noise(const NormalNoisePtr &noise) {
    return std::make_shared<NoiseFunction>(NoiseHolder(noise), 1.0, 1.0);
}

DensityFunctionPtr DensityCommon::noise(const NormalNoisePtr &noise, double xzScale, double yScale) {
    return std::make_shared<NoiseFunction>(NoiseHolder(noise), xzScale, yScale);
}

DensityFunctionPtr DensityCommon::noise(const NoiseHolder &noise, double xzScale, double yScale) {
    return std::make_shared<NoiseFunction>(noise, xzScale, yScale);
}

DensityFunctionPtr DensityCommon::mappedNoise(const NormalNoisePtr &noise, double min, double max) {
    return _mapFromUnitTo(DensityCommon::noise(noise, 1.0, 1.0), min, max);
}

DensityFunctionPtr DensityCommon::mappedNoise(const NormalNoisePtr &noise, double xzScale, double yScale, double min,
                                              double max) {
    return _mapFromUnitTo(DensityCommon::noise(noise, xzScale, yScale), min, max);
}

DensityFunctionPtr DensityCommon::weirdScaledSampler(DensityFunctionPtr input, const NormalNoisePtr &noise,
                                                     RarityValueMapper rarityValueMapper) {
    return std::make_shared<WeirdScaledSamplerFunction>(std::move(input), NoiseHolder(noise), rarityValueMapper);
}

DensityFunctionPtr DensityCommon::shiftA(const NormalNoisePtr &noise) {
    return std::make_shared<ShiftAFunction>(NoiseHolder(noise));
}

DensityFunctionPtr DensityCommon::shiftB(const NormalNoisePtr &noise) {
    return std::make_shared<ShiftBFunction>(NoiseHolder(noise));
}

DensityFunctionPtr DensityCommon::shift(const NormalNoisePtr &noise) {
    return std::make_shared<ShiftFunction>(NoiseHolder(noise));
}

DensityFunctionPtr DensityCommon::shiftedNoise(DensityFunctionPtr shiftX, DensityFunctionPtr shiftY,
                                               DensityFunctionPtr shiftZ, double xzScale, double yScale,
                                               const NoiseHolder &noise) {
    return std::make_shared<ShiftedNoiseFunction>(std::move(shiftX), std::move(shiftY), std::move(shiftZ), xzScale,
                                                  yScale, noise);
}

DensityFunctionPtr DensityCommon::blendAlpha() {
    return std::make_shared<BlendAlphaFunction>();
}

DensityFunctionPtr DensityCommon::blendOffset() {
    return std::make_shared<BlendOffsetFunction>();
}

DensityFunctionPtr DensityCommon::blendDensity(DensityFunctionPtr input) {
    return input;
}

DensityFunctionPtr DensityCommon::_mapFromUnitTo(DensityFunctionPtr input, double min, double max) {
    double midpoint = (min + max) * 0.5;
    double scale = (max - min) * 0.5;
    return add(constant(midpoint), mul(constant(scale), std::move(input)));
}

DensityFunctionPtr DensityCommon::rangeChoice(DensityFunctionPtr input, double minInclusive, double maxExclusive,
                                              DensityFunctionPtr whenInRange, DensityFunctionPtr whenOutOfRange) {
    return std::make_shared<RangeChoiceFunction>(std::move(input), minInclusive, maxExclusive, std::move(whenInRange),
                                                 std::move(whenOutOfRange));
}

DensityFunctionPtr DensityCommon::add(DensityFunctionPtr f1, DensityFunctionPtr f2) {
    return TwoArgumentSimpleFunction::create(TwoArgumentType::ADD, std::move(f1), std::move(f2));
}

DensityFunctionPtr DensityCommon::mul(DensityFunctionPtr f1, DensityFunctionPtr f2) {
    return TwoArgumentSimpleFunction::create(TwoArgumentType::MUL, std::move(f1), std::move(f2));
}

DensityFunctionPtr DensityCommon::min(DensityFunctionPtr f1, DensityFunctionPtr f2) {
    return TwoArgumentSimpleFunction::create(TwoArgumentType::MIN, std::move(f1), std::move(f2));
}

DensityFunctionPtr DensityCommon::max(DensityFunctionPtr f1, DensityFunctionPtr f2) {
    return TwoArgumentSimpleFunction::create(TwoArgumentType::MAX, std::move(f1), std::move(f2));
}

DensityFunctionPtr DensityCommon::zero() {
    return std::make_shared<ConstantFunction>(0.0);
}

DensityFunctionPtr DensityCommon::constant(double value) {
    return std::make_shared<ConstantFunction>(value);
}

DensityFunctionPtr DensityCommon::yClampedGradient(int32_t fromY, int32_t toY, double fromValue, double toValue) {
    return std::make_shared<YClampedGradientFunction>(fromY, toY, fromValue, toValue);
}

DensityFunctionPtr DensityCommon::map(DensityFunctionPtr function, MappedType type) {
    return Mapped::create(type, std::move(function));
}

DensityFunctionPtr DensityCommon::lerp(DensityFunctionPtr factor, double first, DensityFunctionPtr second) {
    return add(mul(std::move(factor), add(std::move(second), constant(-first))), constant(first));
}

DensityFunctionPtr DensityCommon::spline(CubicSplinePtr spline) {
    return std::make_shared<SplineFunction>(std::move(spline));
}

DensityFunctionPtr DensityCommon::spline(DensityFunctionPtr coordinate, std::vector<SplinePoint> points) {
    return spline(std::make_shared<CubicSpline>(std::move(coordinate), std::move(points)));
}

SplinePoint DensityCommon::p(double location, double value, double derivative) {
    SplinePoint point;
    point.mLocation = location;
    point.mFunction = nullptr;
    point.mConstant = value;
    point.mDerivative = derivative;
    return point;
}

SplinePoint DensityCommon::p(double location, DensityFunctionPtr value, double derivative) {
    SplinePoint point;
    point.mLocation = location;
    point.mFunction = std::move(value);
    point.mConstant = 0.0;
    point.mDerivative = derivative;
    return point;
}

DensityFunctionPtr DensityCommon::clamp(DensityFunctionPtr input, double min, double max) {
    return std::make_shared<ClampFunction>(std::move(input), min, max);
}

DensityFunctionPtr DensityCommon::abs(DensityFunctionPtr input) {
    return map(std::move(input), MappedType::ABS);
}

DensityFunctionPtr DensityCommon::square(DensityFunctionPtr input) {
    return map(std::move(input), MappedType::SQUARE);
}

DensityFunctionPtr DensityCommon::cube(DensityFunctionPtr input) {
    return map(std::move(input), MappedType::CUBE);
}

DensityFunctionPtr DensityCommon::halfNegative(DensityFunctionPtr input) {
    return map(std::move(input), MappedType::HALF_NEGATIVE);
}

DensityFunctionPtr DensityCommon::quarterNegative(DensityFunctionPtr input) {
    return map(std::move(input), MappedType::QUARTER_NEGATIVE);
}

DensityFunctionPtr DensityCommon::invert(DensityFunctionPtr input) {
    return map(std::move(input), MappedType::INVERT);
}

DensityFunctionPtr DensityCommon::squeeze(DensityFunctionPtr input) {
    return map(std::move(input), MappedType::SQUEEZE);
}

DensityFunctionPtr DensityCommon::blockY() {
    return std::make_shared<BlockYFunction>();
}
