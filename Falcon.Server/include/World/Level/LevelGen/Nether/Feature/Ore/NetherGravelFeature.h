#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/LevelGen/Nether/Feature/NetherFeature.h"

class NetherGravelFeature : public NetherFeature {
public:
    virtual const BlockState &getState() const;

    virtual int32_t getClusterCount() const;

    virtual int32_t getClusterSize() const;

    virtual int32_t getMinHeight() const;

    virtual int32_t getMaxHeight() const;

    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;
};
