#pragma once

#include "Core/Math/Vector3i.h"
#include "Core/NBT/Tag.h"

#include <cstdint>
#include <random>
#include <vector>

class BlockState;
class Level;
class ServerNetworkHandler;
class SculkWorld;

// one charge on its way through the sculk
struct SculkChargeCursor {
    // a new cursor's facings: spread on every face it can
    static constexpr int32_t ALL_FACES = -1;
    // a sculk block's facings
    static constexpr int32_t SCULK_FACES = -2;

    Vector3i mPosition;
    int32_t mCharge = 0;
    int32_t mUpdateDelay = 0;
    int32_t mDecayDelay = 1;
    int32_t mFacing = ALL_FACES;

    Tag save() const;

    // a missing field reads 0, not the default of a new cursor
    static SculkChargeCursor load(const Tag &data);
};

class SculkSpreader {
public:
    static constexpr size_t MAX_CURSORS = 32;
    static constexpr int32_t MAX_CHARGE = 1000;

    struct Settings {
        bool mWorldGeneration = false;
        int32_t mGrowthSpawnCost = 10;
        int32_t mNoGrowthRadius = 4;
        int32_t mChargeDecayRate = 10;
        int32_t mAdditionalDecayRate = 5;
    };

    SculkSpreader() = default;

    explicit SculkSpreader(const Settings &settings) : mSettings(settings) {}

    // split in cursors of at most MAX_CHARGE, a part past MAX_CURSORS is lost
    void addCursors(const Vector3i &position, int32_t charge);

    const std::vector<SculkChargeCursor> &getCursors() const {
        return mCursors;
    }

    int32_t getTotalCharge() const;

    const Settings &getSettings() const {
        return mSettings;
    }

    // moves every cursor a step from origin, the catalyst, and spends their charge
    void updateCursors(ServerNetworkHandler *owner, Level &level, const Vector3i &origin, std::mt19937 &random,
                       bool convertBlocks);

    void updateCursors(SculkWorld &world, const Vector3i &origin, std::mt19937 &random, bool convertBlocks);

    void clearCursors() {
        mCursors.clear();
    }

    // what sculk can grow over
    bool isReplaceable(const BlockState &state) const;

    // nothing when there is no cursor
    void save(Tag &data) const;

    // only into an empty spreader
    void load(const Tag &data);

private:
    void _updateCursor(SculkWorld &world, const Vector3i &origin, std::mt19937 &random, bool convertBlocks,
                       SculkChargeCursor &cursor);

    bool _findMovement(SculkWorld &world, const Vector3i &from, std::mt19937 &random, Vector3i &to) const;

    Settings mSettings;
    std::vector<SculkChargeCursor> mCursors;
};

// SculkBehaviour: how the charge acts on sculk, on veins, and on any other block
namespace SculkBehaviours {
    enum class Kind { Default, Sculk, Vein };

    Kind kindOf(const BlockState &state);

    int32_t updateDecayDelay(Kind kind, int32_t decayDelay);

    int32_t facingsOf(Kind kind, const BlockState &state, int32_t facings);

    bool canChangeBlockStateOnSpread(Kind kind);

    bool attemptSpreadVein(Kind kind, SculkWorld &world, const Vector3i &position, int32_t facings);

    int32_t attemptUseCharge(Kind kind, SculkWorld &world, const Vector3i &origin, const SculkChargeCursor &cursor,
                             std::mt19937 &random, const SculkSpreader &spreader, bool convertBlocks);

    void onDischarged(Kind kind, SculkWorld &world, const Vector3i &position);

    // a vein's faces as multi_face_direction_bits, by face 0 to 5 (down, up, north, south, west, east)
    int32_t faceBit(int32_t face);

    bool hasFace(const BlockState &state, int32_t face);

    // veins over every face of position, from each of its faces toward every direction
    int32_t spreadVeinsAll(Level &level, const Vector3i &position, bool samePositionOnly);

    int32_t spreadVeinsAll(SculkWorld &world, const Vector3i &position, bool samePositionOnly);
}
