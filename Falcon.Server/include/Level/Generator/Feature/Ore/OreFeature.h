#pragma once

#include "Block/BlockState.h"
#include "Level/Generator/Feature/IFeature.h"

class BlockManager;

class OreFeature : public IFeature {
public:
    enum class ConcentrationType {
        UNIFORM,
        TRIANGLE
    };

    virtual const BlockState &getState(const BlockState &original) const = 0;

    virtual int32_t getClusterCount() const = 0;

    virtual int32_t getClusterSize() const = 0;

    virtual int32_t getMinHeight() const = 0;

    virtual int32_t getMaxHeight() const = 0;

    virtual float getSkipAir() const;

    virtual ConcentrationType getConcentration() const;

    virtual bool isRare() const;

    virtual bool isHostedOre() const;

    virtual bool isIntrusiveDeposit() const;

    virtual const char *getExcludingBiomeTag() const;

    virtual bool canBeReplaced(const BlockState &state) const;

    void apply(ChunkGenerateContext &context) final;

protected:
    static const BlockState &stoneState();

    static const BlockState &deepslateState();

    static const BlockState &netherrackState();

    void spawn(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z);

private:
    static bool _isHostBlock(const BlockState &state);

    bool _canReplace(const BlockState &state) const;

    void _spawnHost(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z);

    void _spawnShape(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z, int32_t clusterSize,
                     const BlockState *hostState);

    bool mReplacesHostBlocks = false;
};
