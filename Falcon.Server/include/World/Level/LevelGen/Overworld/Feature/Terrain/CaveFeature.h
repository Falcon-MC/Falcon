#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/LevelGen/Feature/IFeature.h"

class LevelChunk;

class CaveFeature : public IFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

protected:
    static const int32_t LAVA_LEVEL_OFFSET = 8;
    static const int32_t CHUNK_SIZE = 16;

    static const BlockState &airState();

    static const BlockState &lavaState();

    static bool isLiquidState(const BlockState &state);

    virtual void carveChunk(IRandom &random, int32_t sourceChunkX, int32_t sourceChunkZ, LevelChunk &chunk);

    virtual float getCaveProbability() const;

    virtual int32_t getCaveBound() const;

    virtual int32_t getCaveMaxY() const;

    virtual float getThickness(IRandom &random) const;

    virtual double getYScale() const;

    void createRoom(LevelChunk &chunk, double x, double y, double z, float thickness, double yScale,
                    double floorLevel, int32_t minY, int32_t maxY, int32_t lavaLevel);

    void createTunnel(int64_t tunnelSeed, LevelChunk &chunk, double x, double y, double z,
                      double horizontalRadiusMultiplier, double verticalRadiusMultiplier, float thickness,
                      float horizontalRotation, float verticalRotation, int32_t step, int32_t dist, double yScale,
                      double floorLevel, int32_t minY, int32_t maxY, int32_t lavaLevel);

    bool canReach(double centerX, double centerZ, double x, double z, int32_t currentStep, int32_t distance,
                  float thickness) const;

    bool hasLiquid(LevelChunk &chunk, int32_t xFrom, int32_t xTo, int32_t yFrom, int32_t yTo, int32_t zFrom,
                   int32_t zTo, int32_t maxY) const;

    void carveEllipsoid(LevelChunk &chunk, double x, double y, double z, double horizontalRadius,
                        double verticalRadius, double floorLevel, int32_t minY, int32_t maxY, int32_t lavaLevel);

    void restoreSurfaceIfNeeded(LevelChunk &chunk, int32_t x, int32_t y, int32_t z, bool grassFound);

    int32_t mCarvingRangeChunks = 8;
};
