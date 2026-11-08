#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Protocol/Packets/ClientboundMapItemDataPacket.h"
#include "Protocol/Types/MapPixel.h"

#include <cstdint>
#include <string>
#include <vector>

class Tag;

enum class MapDecorationType : uint8_t {
    MarkerWhite = 0,
    MarkerGreen = 1,
    MarkerRed = 2,
    MarkerBlue = 3,
    XWhite = 4,
    TriangleRed = 5,
    SquareWhite = 6,
    MarkerSign = 7,
    MarkerPink = 8,
    MarkerOrange = 9,
    MarkerYellow = 10,
    MarkerTeal = 11,
    TriangleGreen = 12,
    SmallSquareWhite = 13,
    Mansion = 14,
    Monument = 15,
    NoDraw = 16,
    VillageDesert = 17,
    VillagePlains = 18,
    VillageSavanna = 19,
    VillageSnowy = 20,
    VillageTaiga = 21,
    JungleTemple = 22,
    WitchHut = 23,
    TrialChambers = 24,
};

struct MapItemTrackedActorKey {
    enum class Type : int32_t {
        Entity = 0,
        Block = 1,
    };

    Type mType = Type::Entity;
    int64_t mActorId = -1;
    Vector3i mPosition;

    static MapItemTrackedActorKey entity(int64_t actorId);

    static MapItemTrackedActorKey block(const Vector3i &position);

    bool operator==(const MapItemTrackedActorKey &other) const;
};

struct MapDecorationEntry {
    MapItemTrackedActorKey mKey;
    MapDecorationType mType = MapDecorationType::MarkerWhite;
    int8_t mX = 0;
    int8_t mY = 0;
    int8_t mRotation = 0;
    std::string mLabel;
    // RGBA, white unless the tracked player is in another dimension
    float mColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct MapItemTrackingInfo {
    MapItemTrackedActorKey mKey;
    bool mDirty = true;
    uint32_t mMinX = 0;
    uint32_t mMinY = 0;
    uint32_t mMaxX = 127;
    uint32_t mMaxY = 127;
    uint32_t mTick = 0;
    float mRotation = 0.0f;
    MapDecorationType mDecorationType = MapDecorationType::MarkerWhite;
    int32_t mDimension = 0;

    void markDirty(uint32_t x, uint32_t y);
};

// MapItemSavedData: 128 x 128 ABGR pixels, decorations and tracked actors; scales chain through mParentMapId
class MapItemSavedData {
public:
    static const int32_t MAP_SIZE = 128;
    static const int32_t PIXEL_COUNT = MAP_SIZE * MAP_SIZE;
    static const int8_t MAX_SCALE = 4;
    static const int64_t INVALID_ID = -1;
    // sub_141AE17C0 writes this for a column whose blocks all lack a map colour
    static const uint32_t UNEXPLORED_PIXEL = 0xff000000u;

    explicit MapItemSavedData(int64_t mapId);

    int64_t getMapId() const {
        return mMapId;
    }

    int64_t getParentMapId() const {
        return mParentMapId;
    }

    void setParentMapId(int64_t parentMapId);

    int8_t getScale() const {
        return mScale;
    }

    void setScale(int8_t scale);

    int32_t getDimension() const {
        return mDimension;
    }

    const Vector3i &getOrigin() const {
        return mOrigin;
    }

    // sub_1483ED0B0, with the infinite world settings the game passes
    void setOrigin(const Vector3f &position, int8_t scale, int32_t dimension);

    bool isLocked() const {
        return mLocked;
    }

    void setLocked(bool locked);

    bool hasUnlimitedTracking() const {
        return mUnlimitedTracking;
    }

    void setUnlimitedTracking(bool unlimited);

    bool isPreviewIncomplete() const {
        return mPreviewIncomplete;
    }

    void setPreviewIncomplete(bool incomplete) {
        mPreviewIncomplete = incomplete;
    }

    bool isFullyExplored() const {
        return mFullyExplored;
    }

    bool hasUnexploredPixels() const {
        return mHasUnexploredPixels;
    }

    void refreshUnexploredPixels();

    bool isDirtyForSave() const {
        return mDirtyForSave;
    }

    void clearDirtyForSave() {
        mDirtyForSave = false;
    }

    void markDirty();

    uint32_t getPixel(int32_t x, int32_t y) const;

    // stores a colour and grows the changed area of every tracker; false when the pixel already had it
    bool setPixel(int32_t x, int32_t y, uint32_t color);

    const std::vector<uint32_t> &getPixels() const {
        return mPixels;
    }

    // sub_14228D8A0: client side generation pixels; blank ones and those out of the map are dropped
    bool applyClientPixels(const std::vector<MapPixel> &pixels);

    // the tick counter of MapItem::inventoryTick (sub_141AE31C0): the map renders when it reaches 20
    int64_t &getUpdateCounter() {
        return mUpdateCounter;
    }

    void save(Tag &tag) const;

    void load(const Tag &tag);

    // sub_1483F2E30: a copy through save and load, keeping this map's id
    void copyFrom(const MapItemSavedData &other);

    const std::vector<MapDecorationEntry> &getDecorations() const {
        return mDecorations;
    }

    // sub_1483EFEB0; position holds x, the rotation in degrees, then z
    void addDecoration(MapDecorationType type, const MapItemTrackedActorKey &key, const Vector3f &position,
                       const std::string &label, const float color[4], int32_t rotationNoise);

    // sub_1483F13D0
    void removeDecoration(const MapItemTrackedActorKey &key);

    std::vector<MapItemTrackingInfo> &getTracked() {
        return mTracked;
    }

    MapItemTrackingInfo *findTracked(const MapItemTrackedActorKey &key);

    // sub_1483F1910 without the decoration refresh, which the caller runs
    MapItemTrackingInfo &addTracked(const MapItemTrackedActorKey &key, int32_t dimension, MapDecorationType type);

    // sub_1483F21F0
    void removeTracked(const MapItemTrackedActorKey &key);

    // sub_1483F2F20: every pixel and decoration
    ClientboundMapItemDataPacket buildFullPacket() const;

    // sub_1483F2450: the changed area, otherwise the decorations every fifth call; false when nothing is due
    bool buildUpdatePacket(MapItemTrackingInfo &tracking, ClientboundMapItemDataPacket &packet) const;

    // sub_144169380: the ids of this map and of its parents, which the client links for zooming
    ClientboundMapItemDataPacket buildCreationPacket(const std::vector<int64_t> &mapIds) const;

    static bool isSavedDecoration(MapDecorationType type);

    static bool isFixedRotationDecoration(MapDecorationType type);

    static uint32_t colorToAbgr(const float color[4]);

private:
    ClientboundMapItemDataPacket _buildPacket(int32_t x, int32_t y, int32_t width, int32_t height) const;

    int64_t mUpdateCounter = 0;
    int64_t mMapId;
    int64_t mParentMapId = INVALID_ID;
    bool mFullyExplored = false;
    bool mPreviewIncomplete = false;
    Vector3i mOrigin;
    int32_t mDimension = -1;
    int8_t mScale = 0;
    std::vector<uint32_t> mPixels;
    bool mUnlimitedTracking = false;
    bool mDirtyForSave = false;
    bool mLocked = false;
    std::vector<MapDecorationEntry> mDecorations;
    std::vector<MapItemTrackingInfo> mTracked;
    bool mHasUnexploredPixels = false;
};
