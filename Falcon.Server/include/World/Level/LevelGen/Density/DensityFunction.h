#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

class DensityMarkerState {
public:
    virtual ~DensityMarkerState() = default;
};

class DensityChunkCache {
public:
    template<typename T>
    T *getOrCreate(const void *marker) {
        auto found = mStates.find(marker);
        if (found != mStates.end())
            return static_cast<T *>(found->second.get());

        std::shared_ptr<DensityMarkerState> created = std::make_shared<T>();
        T *raw = static_cast<T *>(created.get());
        mStates.emplace(marker, std::move(created));
        return raw;
    }

    void clear() {
        mStates.clear();
    }

private:
    std::unordered_map<const void *, std::shared_ptr<DensityMarkerState>> mStates;
};

class FunctionContext {
public:
    virtual ~FunctionContext() = default;

    virtual int32_t blockX() const = 0;

    virtual int32_t blockY() const = 0;

    virtual int32_t blockZ() const = 0;

    virtual DensityChunkCache *densityChunkCache() const {
        return nullptr;
    }
};

class MutableFunctionContext : public FunctionContext {
public:
    MutableFunctionContext &set(int32_t blockX, int32_t blockY, int32_t blockZ) {
        mBlockX = blockX;
        mBlockY = blockY;
        mBlockZ = blockZ;
        return *this;
    }

    int32_t blockX() const override {
        return mBlockX;
    }

    int32_t blockY() const override {
        return mBlockY;
    }

    int32_t blockZ() const override {
        return mBlockZ;
    }

private:
    int32_t mBlockX = 0;
    int32_t mBlockY = 0;
    int32_t mBlockZ = 0;
};

class CellFunctionContext : public FunctionContext {
public:
    explicit CellFunctionContext(DensityChunkCache *chunkCache) : mChunkCache(chunkCache) {}

    CellFunctionContext &set(int32_t worldX, int32_t worldY, int32_t worldZ) {
        mWorldX = worldX;
        mWorldY = worldY;
        mWorldZ = worldZ;
        return *this;
    }

    int32_t blockX() const override {
        return mWorldX;
    }

    int32_t blockY() const override {
        return mWorldY;
    }

    int32_t blockZ() const override {
        return mWorldZ;
    }

    DensityChunkCache *densityChunkCache() const override {
        return mChunkCache;
    }

private:
    DensityChunkCache *mChunkCache;
    int32_t mWorldX = 0;
    int32_t mWorldY = 0;
    int32_t mWorldZ = 0;
};

class MutableChunkCacheContext : public FunctionContext {
public:
    MutableChunkCacheContext &withCache(DensityChunkCache *chunkCache) {
        mChunkCache = chunkCache;
        return *this;
    }

    MutableChunkCacheContext &set(int32_t blockX, int32_t blockY, int32_t blockZ) {
        mBlockX = blockX;
        mBlockY = blockY;
        mBlockZ = blockZ;
        return *this;
    }

    int32_t blockX() const override {
        return mBlockX;
    }

    int32_t blockY() const override {
        return mBlockY;
    }

    int32_t blockZ() const override {
        return mBlockZ;
    }

    DensityChunkCache *densityChunkCache() const override {
        return mChunkCache;
    }

private:
    DensityChunkCache *mChunkCache = nullptr;
    int32_t mBlockX = 0;
    int32_t mBlockY = 0;
    int32_t mBlockZ = 0;
};

class DensityFunction {
public:
    virtual ~DensityFunction() = default;

    virtual double compute(FunctionContext &context) = 0;

    virtual double minValue() const = 0;

    virtual double maxValue() const = 0;
};

using DensityFunctionPtr = std::shared_ptr<DensityFunction>;
