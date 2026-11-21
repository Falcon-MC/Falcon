#include "World/Level/Map/MapItemSavedData.h"

#include "Core/NBT/Tag.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {
    // 0, 6, 13 and 16 follow players and are rebuilt every tick, so a save leaves them out
    const uint32_t UNSAVED_DECORATIONS = 0x12041;

    // the game and the following entries of the exploration table: their markers never turn
    const uint8_t FIXED_ROTATION_DECORATIONS[] = {15, 14, 4, 20, 21, 18, 19, 17, 22, 23, 24, 25, 26, 27, 28, 29};

    // not a game key: the game ignores it and keeps the map as a locked one
    const char *const IMAGE_TAG = "falconImageMap";

    const int32_t FAR_TRACKING_LIMIT = 320;
    const int8_t FIXED_ROTATION = 8;

    int32_t floorToInt(float value) {
        const int32_t truncated = (int32_t) value;
        return value < (float) truncated ? truncated - 1 : truncated;
    }

    int8_t toMapCoordinate(int32_t offset) {
        int32_t coordinate = 2 * offset;
        if (offset >= 63)
            coordinate = 127;
        if (offset < -62)
            coordinate = -128;
        return (int8_t) coordinate;
    }

    int8_t toMapRotation(float degrees) {
        return (int8_t) (int32_t) (((degrees + (degrees < 0.0f ? -8.0f : 8.0f)) * 16.0f) / 360.0f);
    }

    bool outsideMap(int32_t dx, int32_t dz) {
        return ((uint32_t) (dx + 64) | (uint32_t) (dz + 64)) >= 128;
    }

    Tag keyToTag(const MapItemTrackedActorKey &key) {
        Tag tag = Tag::ofCompound();
        tag.putInt("type", (int32_t) key.mType);
        if (key.mType == MapItemTrackedActorKey::Type::Entity) {
            tag.putLong("entityId", key.mActorId);
        } else if (key.mType == MapItemTrackedActorKey::Type::Block) {
            tag.putInt("blockX", key.mPosition.x);
            tag.putInt("blockY", key.mPosition.y);
            tag.putInt("blockZ", key.mPosition.z);
        }
        return tag;
    }
}

MapItemTrackedActorKey MapItemTrackedActorKey::entity(int64_t actorId) {
    MapItemTrackedActorKey key;
    key.mType = Type::Entity;
    key.mActorId = actorId;
    return key;
}

MapItemTrackedActorKey MapItemTrackedActorKey::block(const Vector3i &position) {
    MapItemTrackedActorKey key;
    key.mType = Type::Block;
    key.mPosition = position;
    return key;
}

bool MapItemTrackedActorKey::operator==(const MapItemTrackedActorKey &other) const {
    if (mType != other.mType)
        return false;

    if (mType == Type::Entity)
        return mActorId == other.mActorId;

    return mPosition == other.mPosition;
}

void MapItemTrackingInfo::markDirty(uint32_t x, uint32_t y) {
    if (!mDirty) {
        mDirty = true;
        mMinX = mMaxX = x;
        mMinY = mMaxY = y;
        return;
    }

    mMinX = std::min(mMinX, x);
    mMinY = std::min(mMinY, y);
    mMaxX = std::max(mMaxX, x);
    mMaxY = std::max(mMaxY, y);
}

MapItemSavedData::MapItemSavedData(int64_t mapId) : mMapId(mapId), mPixels((size_t) PIXEL_COUNT, 0) {}

void MapItemSavedData::markDirty() {
    mDirtyForSave = true;
}

void MapItemSavedData::setParentMapId(int64_t parentMapId) {
    if (mParentMapId == parentMapId)
        return;

    mParentMapId = parentMapId;
    markDirty();
}

void MapItemSavedData::setScale(int8_t scale) {
    if (mScale == scale)
        return;

    mScale = scale;
    markDirty();
}

void MapItemSavedData::setOrigin(const Vector3f &position, int8_t scale, int32_t dimension) {
    const int32_t size = MAP_SIZE << scale;
    const int32_t centerX = floorToInt((position.x + 64.0f) / (float) size) * size + (size >> 1) - 64;
    const int32_t centerZ = floorToInt((position.z + 64.0f) / (float) size) * size + (size >> 1) - 64;

    const Vector3i origin(centerX, 0, centerZ);
    if (mOrigin != origin || mDimension != dimension)
        markDirty();

    mOrigin = origin;
    mDimension = dimension;
}

void MapItemSavedData::setLocked(bool locked) {
    if (mLocked == locked)
        return;

    mLocked = locked;
    markDirty();
}

void MapItemSavedData::setUnlimitedTracking(bool unlimited) {
    if (mUnlimitedTracking == unlimited)
        return;

    mUnlimitedTracking = unlimited;
    markDirty();
}

void MapItemSavedData::setImage(bool image) {
    if (mImage == image)
        return;

    mImage = image;
    markDirty();
}

void MapItemSavedData::refreshUnexploredPixels() {
    const size_t unexplored = (size_t) std::count(mPixels.begin(), mPixels.end(), UNEXPLORED_PIXEL);
    mHasUnexploredPixels = unexplored >= (size_t) (int64_t) ((float) mPixels.size() * 0.005f);
}

uint32_t MapItemSavedData::getPixel(int32_t x, int32_t y) const {
    if (x < 0 || y < 0 || x >= MAP_SIZE || y >= MAP_SIZE)
        return 0;

    return mPixels[(size_t) (x + y * MAP_SIZE)];
}

bool MapItemSavedData::setPixel(int32_t x, int32_t y, uint32_t color) {
    if (x < 0 || y < 0 || x >= MAP_SIZE || y >= MAP_SIZE)
        return false;

    uint32_t &pixel = mPixels[(size_t) (x + y * MAP_SIZE)];
    if (pixel == color)
        return false;

    pixel = color;
    markDirty();
    for (MapItemTrackingInfo &tracking: mTracked)
        tracking.markDirty((uint32_t) x, (uint32_t) y);
    return true;
}

bool MapItemSavedData::writePixels(int32_t x, int32_t y, int32_t width, int32_t height, const uint32_t *pixels,
                                   MapPixelArea &changed) {
    changed = MapPixelArea();
    if (pixels == nullptr || width <= 0 || height <= 0 || x < 0 || y < 0 || x > MAP_SIZE - width
        || y > MAP_SIZE - height)
        return false;

    for (int32_t row = 0; row < height; ++row) {
        for (int32_t column = 0; column < width; ++column) {
            uint32_t &pixel = mPixels[(size_t) (x + column + (y + row) * MAP_SIZE)];
            const uint32_t color = pixels[(size_t) (column + row * width)];
            if (pixel == color)
                continue;

            pixel = color;
            if (changed.isEmpty()) {
                changed.mMinX = changed.mMaxX = x + column;
                changed.mMinY = changed.mMaxY = y + row;
                continue;
            }
            changed.mMinX = std::min(changed.mMinX, x + column);
            changed.mMaxX = std::max(changed.mMaxX, x + column);
            changed.mMinY = std::min(changed.mMinY, y + row);
            changed.mMaxY = std::max(changed.mMaxY, y + row);
        }
    }

    if (changed.isEmpty())
        return false;

    markDirty();
    return true;
}

bool MapItemSavedData::applyClientPixels(const std::vector<MapPixel> &pixels) {
    if (mImage)
        return false;

    bool changed = false;
    for (const MapPixel &pixel: pixels) {
        // the game indexes its array without a bound check
        if (pixel.mPixel == 0 || pixel.mIndex < 0 || pixel.mIndex >= PIXEL_COUNT)
            continue;

        changed |= setPixel(pixel.mIndex & 0x7f, pixel.mIndex >> 7, (uint32_t) pixel.mPixel);
    }
    return changed;
}

void MapItemSavedData::save(Tag &tag) const {
    tag.putLong("mapId", mMapId);
    tag.putLong("parentMapId", mParentMapId);
    tag.putByte("dimension", (int8_t) mDimension);
    tag.putInt("xCenter", mOrigin.x);
    tag.putInt("zCenter", mOrigin.z);
    tag.putByte("scale", mScale);
    tag.putShort("width", (int16_t) MAP_SIZE);
    tag.putShort("height", (int16_t) MAP_SIZE);
    tag.putByte("unlimitedTracking", mUnlimitedTracking ? 1 : 0);

    // the pixels are written as they lie in memory, four little endian bytes each
    std::vector<int8_t> colors(mPixels.size() * 4);
    for (size_t index = 0; index < mPixels.size(); ++index) {
        const uint32_t pixel = mPixels[index];
        colors[index * 4] = (int8_t) (pixel & 0xff);
        colors[index * 4 + 1] = (int8_t) ((pixel >> 8) & 0xff);
        colors[index * 4 + 2] = (int8_t) ((pixel >> 16) & 0xff);
        colors[index * 4 + 3] = (int8_t) ((pixel >> 24) & 0xff);
    }
    tag.put("colors", Tag::ofByteArray(std::move(colors)));
    tag.putByte("fullyExplored", mFullyExplored ? 1 : 0);
    if (mPreviewIncomplete)
        tag.putByte("previewIncomplete", 1);

    Tag decorations = Tag::ofList(Tag::Type::Compound);
    for (const MapDecorationEntry &decoration: mDecorations) {
        if (!isSavedDecoration(decoration.mType))
            continue;

        Tag data = Tag::ofCompound();
        data.putInt("type", (int32_t) decoration.mType);
        data.putInt("x", decoration.mX);
        data.putInt("y", decoration.mY);
        data.putInt("rot", decoration.mRotation);

        Tag entry = Tag::ofCompound();
        entry.put("data", std::move(data));
        entry.put("key", keyToTag(decoration.mKey));
        decorations.addToList(std::move(entry));
    }
    tag.put("decorations", std::move(decorations));
    tag.putByte("mapLocked", mLocked ? 1 : 0);
    if (mImage)
        tag.putByte(IMAGE_TAG, 1);
}

void MapItemSavedData::load(const Tag &tag) {
    mParentMapId = tag.getLong("parentMapId", 0);
    mDimension = (uint8_t) tag.getByte("dimension", 0);
    mOrigin = Vector3i(tag.getInt("xCenter", 0), 0, tag.getInt("zCenter", 0));
    const int8_t storedScale = tag.getByte("scale", 0);
    mScale = std::min<int8_t>(std::max<int8_t>(storedScale, 0), MAX_SCALE);
    // the game only reports the mismatch; the largest scale has no parent
    if (mParentMapId != INVALID_ID && mScale == MAX_SCALE)
        mParentMapId = INVALID_ID;

    mUnlimitedTracking = tag.getByte("unlimitedTracking", 0) != 0;
    mPreviewIncomplete = tag.getByte("previewIncomplete", 0) != 0;

    const int16_t width = tag.getShort("width", 0);
    const int16_t height = tag.getShort("height", 0);
    const Tag *colorsTag = tag.get("colors");
    const std::vector<int8_t> empty;
    const std::vector<int8_t> &colors = colorsTag != nullptr && colorsTag->getType() == Tag::Type::ByteArray
                                                ? colorsTag->asByteArray()
                                                : empty;
    if (width == MAP_SIZE && height == MAP_SIZE) {
        std::fill(mPixels.begin(), mPixels.end(), 0);
        const size_t count = std::min(colors.size() / 4, mPixels.size());
        for (size_t index = 0; index < count; ++index) {
            mPixels[index] = (uint32_t) (uint8_t) colors[index * 4] | ((uint32_t) (uint8_t) colors[index * 4 + 1] << 8)
                             | ((uint32_t) (uint8_t) colors[index * 4 + 2] << 16)
                             | ((uint32_t) (uint8_t) colors[index * 4 + 3] << 24);
        }
    } else if (height > 0 && width > 0) {
        // older maps kept one palette byte per pixel, centred when smaller than 128
        const int32_t offsetX = (MAP_SIZE - width) / 2;
        const int32_t offsetY = (MAP_SIZE - height) / 2;
        for (int32_t y = 0; y < height; ++y) {
            for (int32_t x = 0; x < width; ++x) {
                const size_t source = (size_t) (y * width + x);
                if (source >= colors.size() || x + offsetX < 0 || x + offsetX >= MAP_SIZE || y + offsetY < 0
                    || y + offsetY >= MAP_SIZE)
                    continue;

                mPixels[(size_t) ((y + offsetY) * MAP_SIZE + x + offsetX)] = (uint8_t) colors[source];
            }
        }
    }

    if (tag.contains("fullyExplored"))
        mFullyExplored = tag.getByte("fullyExplored", 0) != 0;

    const Tag *decorations = tag.get("decorations");
    if (decorations != nullptr && decorations->isList()) {
        for (const Tag &entry: decorations->getList()) {
            if (!entry.isCompound())
                continue;

            const Tag *data = entry.get("data");
            const Tag *key = entry.get("key");
            if (data == nullptr || !data->isCompound() || key == nullptr || !key->isCompound())
                continue;

            MapDecorationEntry decoration;
            decoration.mType = (MapDecorationType) (uint8_t) data->getInt("type", 0);
            decoration.mX = (int8_t) data->getInt("x", 0);
            decoration.mY = (int8_t) data->getInt("y", 0);
            decoration.mRotation = (int8_t) data->getInt("rot", 0);

            const int32_t keyType = key->getInt("type", 0);
            decoration.mKey.mType = (MapItemTrackedActorKey::Type) keyType;
            if (keyType == 0) {
                decoration.mKey.mActorId = key->getLong("entityId", 0);
            } else if (keyType == 1) {
                decoration.mKey.mPosition = Vector3i(key->getInt("blockX", 0), key->getInt("blockY", 0),
                                                     key->getInt("blockZ", 0));
            }
            mDecorations.push_back(std::move(decoration));
        }
    }

    if (tag.contains("mapLocked"))
        mLocked = tag.getByte("mapLocked", 0) != 0;
    mImage = tag.getByte(IMAGE_TAG, 0) != 0;

    refreshUnexploredPixels();
}

void MapItemSavedData::copyFrom(const MapItemSavedData &other) {
    Tag data = Tag::ofCompound();
    other.save(data);
    mDecorations.clear();
    load(data);
    markDirty();
}

void MapItemSavedData::addDecoration(MapDecorationType type, const MapItemTrackedActorKey &key,
                                     const Vector3f &position, const std::string &label, const float color[4],
                                     int32_t rotationNoise) {
    const float scale = (float) (1 << mScale);
    const int32_t dx = (int32_t) std::round((position.x - (float) mOrigin.x) / scale);
    const int32_t dz = (int32_t) std::round((position.z - (float) mOrigin.z) / scale);
    int8_t x = toMapCoordinate(dx);
    int8_t y = toMapCoordinate(dz);

    if (type != MapDecorationType::MarkerWhite && !mUnlimitedTracking && outsideMap(dx, dz)) {
        removeDecoration(key);
        return;
    }

    int8_t rotation = FIXED_ROTATION;
    if (!isFixedRotationDecoration(type)) {
        rotation = toMapRotation(position.y);
        // the Nether spins the marker from where it last pointed
        if (mDimension == 1) {
            for (const MapDecorationEntry &decoration: mDecorations) {
                if (decoration.mKey == key)
                    rotation = decoration.mRotation;
            }
            rotation = (int8_t) (rotation + rotationNoise);
        }
    }

    if (type == MapDecorationType::MarkerWhite && outsideMap(dx, dz)) {
        type = MapDecorationType::SquareWhite;
        if (std::abs(dx) >= FAR_TRACKING_LIMIT || std::abs(dz) >= FAR_TRACKING_LIMIT)
            type = mUnlimitedTracking ? MapDecorationType::SmallSquareWhite : MapDecorationType::NoDraw;
    }

    if (type == MapDecorationType::NoDraw) {
        removeDecoration(key);
        return;
    }

    MapDecorationEntry entry;
    entry.mKey = key;
    entry.mType = type;
    entry.mX = x;
    entry.mY = y;
    entry.mRotation = rotation;
    entry.mLabel = label;
    std::memcpy(entry.mColor, color, sizeof(entry.mColor));

    for (MapDecorationEntry &decoration: mDecorations) {
        if (decoration.mKey == key) {
            decoration = std::move(entry);
            return;
        }
    }

    mDecorations.push_back(std::move(entry));
    if (isSavedDecoration(type))
        markDirty();
}

void MapItemSavedData::removeDecoration(const MapItemTrackedActorKey &key) {
    const auto removed = std::remove_if(
            mDecorations.begin(), mDecorations.end(),
            [&key](const MapDecorationEntry &decoration) { return decoration.mKey == key; });
    if (removed == mDecorations.end())
        return;

    for (auto it = removed; it != mDecorations.end(); ++it) {
        if (isSavedDecoration(it->mType))
            markDirty();
    }
    mDecorations.erase(removed, mDecorations.end());
}

MapItemTrackingInfo *MapItemSavedData::findTracked(const MapItemTrackedActorKey &key) {
    for (MapItemTrackingInfo &tracking: mTracked) {
        if (tracking.mKey == key)
            return &tracking;
    }
    return nullptr;
}

MapItemTrackingInfo &MapItemSavedData::addTracked(const MapItemTrackedActorKey &key, int32_t dimension,
                                                  MapDecorationType type) {
    MapItemTrackingInfo *tracking = findTracked(key);
    if (tracking == nullptr) {
        MapItemTrackingInfo created;
        created.mKey = key;
        created.mDimension = dimension;
        mTracked.push_back(created);
        tracking = &mTracked.back();
    }

    tracking->mDecorationType = type;
    if (isSavedDecoration(type))
        markDirty();
    return *tracking;
}

void MapItemSavedData::removeTracked(const MapItemTrackedActorKey &key) {
    mTracked.erase(std::remove_if(mTracked.begin(), mTracked.end(),
                                  [&key](const MapItemTrackingInfo &tracking) { return tracking.mKey == key; }),
                   mTracked.end());
    removeDecoration(key);
}

ClientboundMapItemDataPacket MapItemSavedData::_buildPacket(int32_t x, int32_t y, int32_t width, int32_t height) const {
    ClientboundMapItemDataPacket packet;
    packet.mUniqueMapId = mMapId;
    packet.mDimensionId = mDimension;
    packet.mLocked = mLocked;
    packet.mOrigin = mOrigin;
    packet.mHasScale = true;
    packet.mScale = mScale;

    packet.mHasTrackedObjects = true;
    packet.mHasDecorations = true;
    for (const MapDecorationEntry &decoration: mDecorations) {
        MapTrackedObject object;
        object.mType = decoration.mKey.mType == MapItemTrackedActorKey::Type::Block ? MapTrackedObjectType::Block
                                                                                    : MapTrackedObjectType::Entity;
        object.mActorId = decoration.mKey.mActorId;
        object.mPosition = decoration.mKey.mPosition;
        packet.mTrackedObjects.push_back(object);

        MapDecoration network;
        network.mImage = (int32_t) decoration.mType;
        network.mRotation = (uint8_t) decoration.mRotation;
        network.mXOffset = (uint8_t) decoration.mX;
        network.mYOffset = (uint8_t) decoration.mY;
        network.mLabel = decoration.mLabel;
        network.mColor = (int32_t) colorToAbgr(decoration.mColor);
        packet.mDecorations.push_back(network);
    }

    if (width <= 0 || height <= 0)
        return packet;

    packet.mHasWidth = packet.mHasHeight = packet.mHasXOffset = packet.mHasYOffset = packet.mHasColors = true;
    packet.mWidth = width;
    packet.mHeight = height;
    packet.mXOffset = x;
    packet.mYOffset = y;
    packet.mColors.reserve((size_t) (width * height));
    for (int32_t row = 0; row < height; ++row) {
        for (int32_t column = 0; column < width; ++column)
            packet.mColors.push_back((int32_t) getPixel(x + column, y + row));
    }
    return packet;
}

ClientboundMapItemDataPacket MapItemSavedData::buildFullPacket() const {
    return _buildPacket(0, 0, MAP_SIZE, MAP_SIZE);
}

bool MapItemSavedData::buildUpdatePacket(MapItemTrackingInfo &tracking, ClientboundMapItemDataPacket &packet) const {
    if (tracking.mDirty) {
        tracking.mDirty = false;
        packet = _buildPacket((int32_t) tracking.mMinX, (int32_t) tracking.mMinY,
                              (int32_t) (tracking.mMaxX - tracking.mMinX + 1),
                              (int32_t) (tracking.mMaxY - tracking.mMinY + 1));
        return true;
    }

    const uint32_t tick = tracking.mTick++;
    if (tick % 5 != 0)
        return false;

    packet = _buildPacket(0, 0, 0, 0);
    return true;
}

ClientboundMapItemDataPacket MapItemSavedData::buildAreaPacket(const MapPixelArea &area) const {
    if (area.isEmpty())
        return _buildPacket(0, 0, 0, 0);

    return _buildPacket(area.mMinX, area.mMinY, area.mMaxX - area.mMinX + 1, area.mMaxY - area.mMinY + 1);
}

ClientboundMapItemDataPacket MapItemSavedData::buildCreationPacket(const std::vector<int64_t> &mapIds) const {
    ClientboundMapItemDataPacket packet;
    packet.mUniqueMapId = mMapId;
    packet.mDimensionId = mDimension;
    packet.mLocked = mLocked;
    packet.mOrigin = mOrigin;
    packet.mHasTrackedActorIds = true;
    packet.mTrackedActorIds = mapIds;
    packet.mHasScale = true;
    packet.mScale = mScale;
    return packet;
}

bool MapItemSavedData::isSavedDecoration(MapDecorationType type) {
    const uint32_t value = (uint32_t) type;
    return value > 16 || ((UNSAVED_DECORATIONS >> value) & 1) == 0;
}

bool MapItemSavedData::isFixedRotationDecoration(MapDecorationType type) {
    for (const uint8_t fixed: FIXED_ROTATION_DECORATIONS) {
        if ((uint8_t) type == fixed)
            return true;
    }
    return false;
}

uint32_t MapItemSavedData::colorToAbgr(const float color[4]) {
    uint32_t packed = 0;
    for (int channel = 3; channel >= 0; --channel) {
        const float clamped = std::min(1.0f, std::max(0.0f, color[channel]));
        packed = (packed << 8) | (uint32_t) (int32_t) (clamped * 255.0f);
    }
    return packed;
}
