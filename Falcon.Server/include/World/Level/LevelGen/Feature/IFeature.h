#pragma once

#include "World/Level/LevelGen/Feature/ChunkGenerateContext.h"
#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

#include <cstdint>
#include <memory>
#include <string>

class BlockManager;
class BlockState;

class IFeature {
public:
    virtual ~IFeature() = default;

    virtual const char *name() const = 0;

    virtual const char *identifier() const;

    virtual void apply(ChunkGenerateContext &context) = 0;

    void setRoot(BlockManager *root) { mRoot = root; }

    static int32_t javaStringHash(const char *text);

    static int32_t randomRange(IRandom &random, int32_t start, int32_t end);

    static bool isSupportDirt(const BlockState &state);

    static bool isSupportGrass(const BlockState &state);

protected:
    void queueObject(const BlockManager &object);

    BlockManager *mRoot = nullptr;
    XoroshiroRandom mRandom{0};
};

using IFeaturePtr = std::shared_ptr<IFeature>;
