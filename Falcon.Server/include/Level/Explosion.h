#pragma once

#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

class Level;
class ServerActor;
class ServerNetworkHandler;

class Explosion {
public:
    Explosion(ServerNetworkHandler &owner, Level &level, const Vector3f &center, double size,
              const ServerActor *sourceActor, bool fromTnt);

    void setFireChance(double fireChance) {
        mFireChance = fireChance;
    }

    void setIncendiary(bool incendiary);

    bool explode();

    bool explodeWithoutBlocks();

    bool explodeA();

    bool explodeB();

    static float getBlockDensity(Level &level, const Vector3f &source, const AxisAlignedBB &boundingBox);

    static bool isRayCollidingWithBlocks(Level &level, double srcX, double srcY, double srcZ, double dstX,
                                         double dstY, double dstZ, double stepSize);

private:
    static int64_t _key(const Vector3i &position);

    bool _findAffectedBlocks();

    bool _allowByPlugins();

    static float _calculateEntityDamage(double doubleRadius, double impact);

    float _scaleDamageForDifficulty(float damage) const;

    void _damageEntities();

    bool _isInsideWater(const Vector3f &position) const;

    void _destroyBlocks();

    void _ignite();

    void _playEffects();

    ServerNetworkHandler &mOwner;
    Level &mLevel;
    Vector3f mSource;
    double mSize;
    double mFireChance = 0.0;
    const ServerActor *mSourceActor;
    bool mFromTnt;
    std::vector<Vector3i> mAffectedBlocks;
    std::unordered_set<int64_t> mAffectedKeys;
    std::vector<Vector3i> mFireIgnitions;
    std::vector<Vector3i> mSmokePositions;
};
