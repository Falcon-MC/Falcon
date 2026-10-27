#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"
#include "World/Level/LevelGen/Feature/ObjectGenerator.h"

#include <vector>

class CountGenerateFeature : public IFeature {
public:
    virtual int32_t getBase() const = 0;

    virtual int32_t getRandom() const = 0;

    virtual void populate(ChunkGenerateContext &context, IRandom &random) = 0;

    void apply(ChunkGenerateContext &context) override;
};

class ObjectGeneratorFeature : public IFeature {
public:
    virtual ObjectGeneratorPtr getGenerator(IRandom &random) = 0;

    virtual int32_t getMin() const;

    virtual int32_t getMax() const;

    virtual bool canSpawnHere(int32_t biomeId) const;

    void apply(ChunkGenerateContext &context) override;

protected:
    virtual bool checkBlock(const BlockState &state, int32_t y) const;
};

class GriddedFeature : public ObjectGeneratorFeature {
public:
    virtual int32_t getSplit() const;

    virtual int32_t getDistanceToNextField() const;

    void apply(ChunkGenerateContext &context) override;

protected:
    int32_t splitLength() const;
};

class MultiFeatureWrapper : public IFeature {
public:
    virtual std::vector<const char *> getFeatures() const = 0;

    void apply(ChunkGenerateContext &context) override;
};

class LegacyTreeGeneratorFeature : public IFeature {
public:
    virtual TreeGeneratorPtr getGenerator(IRandom &random) = 0;

    virtual int32_t getMin() const;

    virtual int32_t getMax() const;

    virtual float getBeeNestChance() const;

    virtual bool canSpawnHere(int32_t biomeId) const;

    void apply(ChunkGenerateContext &context) override;
};
