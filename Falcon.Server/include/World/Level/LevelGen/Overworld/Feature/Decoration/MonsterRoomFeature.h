#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"
#include "World/Level/LevelGen/Feature/ObjectGenerator.h"

class MonsterRoomFeature : public IFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

    static void setGeneratorFactory(ObjectGeneratorPtr (*factory)());
};
