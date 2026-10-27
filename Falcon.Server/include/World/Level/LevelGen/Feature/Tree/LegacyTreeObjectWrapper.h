#pragma once

#include "World/Level/LevelGen/Feature/Tree/LegacyTreeObject.h"

#include <memory>

class LegacyTreeObjectWrapper : public VanillaTreeObject {
public:
    LegacyTreeObjectWrapper() = default;

    explicit LegacyTreeObjectWrapper(const std::shared_ptr<LegacyTreeObject> &legacyTreeGenerator);

    const std::shared_ptr<LegacyTreeObject> &getLegacyTreeGenerator() const { return mLegacyTreeGenerator; }

    void setLegacyTreeGenerator(const std::shared_ptr<LegacyTreeObject> &generator) {
        mLegacyTreeGenerator = generator;
    }

    bool generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) override;

private:
    std::shared_ptr<LegacyTreeObject> mLegacyTreeGenerator;
};
