#include "Level/TickingAreaManager.h"

#include "Core/Utility/UUID.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <random>
#include <utility>

namespace {
    const int32_t CHUNK_SIZE = 16;
    const int32_t CHUNK_SHIFT = 4;
    const uint64_t LOADER_ID_FLAG = 1ULL << 63;
    const char *const DEFAULT_NAME_PREFIX = "Area";

    int64_t packColumn(int32_t chunkX, int32_t chunkZ) {
        return ((int64_t) chunkX << 32) | (uint32_t) chunkZ;
    }
}

int32_t TickingArea::getCenterChunkX() const {
    return ((mMinX + mMaxX) / 2) >> CHUNK_SHIFT;
}

int32_t TickingArea::getCenterChunkZ() const {
    return ((mMinZ + mMaxZ) / 2) >> CHUNK_SHIFT;
}

int32_t TickingArea::getRadius() const {
    return (mMaxX - mMinX) / (2 * CHUNK_SIZE);
}

bool TickingArea::containsChunk(int32_t chunkX, int32_t chunkZ) const {
    if (!mCircle)
        return chunkX >= (mMinX >> CHUNK_SHIFT) && chunkX <= (mMaxX >> CHUNK_SHIFT)
               && chunkZ >= (mMinZ >> CHUNK_SHIFT) && chunkZ <= (mMaxZ >> CHUNK_SHIFT);

    const int32_t dx = chunkX - getCenterChunkX();
    const int32_t dz = chunkZ - getCenterChunkZ();
    const int32_t radius = getRadius();
    return dx * dx + dz * dz <= radius * radius;
}

bool TickingArea::containsBlock(const Vector3i &position) const {
    return containsChunk(position.x >> CHUNK_SHIFT, position.z >> CHUNK_SHIFT);
}

std::vector<int64_t> TickingArea::getColumns() const {
    std::vector<int64_t> columns;
    for (int32_t chunkX = mMinX >> CHUNK_SHIFT; chunkX <= (mMaxX >> CHUNK_SHIFT); ++chunkX) {
        for (int32_t chunkZ = mMinZ >> CHUNK_SHIFT; chunkZ <= (mMaxZ >> CHUNK_SHIFT); ++chunkZ) {
            if (containsChunk(chunkX, chunkZ))
                columns.push_back(packColumn(chunkX, chunkZ));
        }
    }
    return columns;
}

Tag TickingArea::toNbt() const {
    Tag data = Tag::ofCompound();
    data.putInt("Dimension", Dimension::toId(mDimension));
    data.putByte("IsCircle", mCircle ? 1 : 0);
    data.putInt("MaxX", mMaxX);
    data.putInt("MaxZ", mMaxZ);
    data.putInt("MinX", mMinX);
    data.putInt("MinZ", mMinZ);
    data.putString("Name", mName);
    data.putByte("Preload", mPreload ? 1 : 0);
    return data;
}

TickingArea TickingArea::fromNbt(const std::string &id, const Tag &data) {
    TickingArea area;
    area.mId = id;
    area.mName = data.getString("Name", std::string());
    area.mDimension = Dimension::fromId(data.getInt("Dimension", 0));
    area.mCircle = data.getByte("IsCircle", 0) != 0;
    area.mMinX = data.getInt("MinX", 0);
    area.mMinZ = data.getInt("MinZ", 0);
    area.mMaxX = data.getInt("MaxX", 0);
    area.mMaxZ = data.getInt("MaxZ", 0);
    area.mPreload = data.getByte("Preload", 0) != 0;
    return area;
}

TickingArea TickingArea::box(const Vector3i &from, const Vector3i &to) {
    TickingArea area;
    area.mMinX = (std::min(from.x, to.x) >> CHUNK_SHIFT) * CHUNK_SIZE;
    area.mMinZ = (std::min(from.z, to.z) >> CHUNK_SHIFT) * CHUNK_SIZE;
    area.mMaxX = (std::max(from.x, to.x) >> CHUNK_SHIFT) * CHUNK_SIZE + CHUNK_SIZE - 1;
    area.mMaxZ = (std::max(from.z, to.z) >> CHUNK_SHIFT) * CHUNK_SIZE + CHUNK_SIZE - 1;
    return area;
}

TickingArea TickingArea::circle(const Vector3i &center, int32_t radius) {
    TickingArea area;
    area.mCircle = true;
    area.mMinX = ((center.x >> CHUNK_SHIFT) - radius) * CHUNK_SIZE;
    area.mMinZ = ((center.z >> CHUNK_SHIFT) - radius) * CHUNK_SIZE;
    area.mMaxX = ((center.x >> CHUNK_SHIFT) + radius) * CHUNK_SIZE;
    area.mMaxZ = ((center.z >> CHUNK_SHIFT) + radius) * CHUNK_SIZE;
    return area;
}

void TickingAreaManager::load(ServerNetworkHandler &owner) {
    for (const TickingArea &area: mAreas)
        _detach(owner, area);
    mAreas.clear();

    for (const std::pair<std::string, Tag> &entry: owner.getDimension(DimensionType::Overworld).loadTickingAreas()) {
        mAreas.push_back(TickingArea::fromNbt(entry.first, entry.second));
        _attach(owner, mAreas.back());
    }
}

bool TickingAreaManager::hasName(const std::string &name) const {
    return std::any_of(mAreas.begin(), mAreas.end(), [&name](const TickingArea &area) {
        return area.mName == name;
    });
}

std::string TickingAreaManager::nextDefaultName() const {
    for (size_t index = 0;; ++index) {
        const std::string name = DEFAULT_NAME_PREFIX + std::to_string(index);
        if (!hasName(name))
            return name;
    }
}

const TickingArea &TickingAreaManager::add(ServerNetworkHandler &owner, TickingArea area) {
    area.mId = _randomId();
    mAreas.push_back(std::move(area));

    const TickingArea &added = mAreas.back();
    owner.getDimension(DimensionType::Overworld).saveTickingArea(added.mId, added.toNbt());
    _attach(owner, added);
    return added;
}

std::vector<TickingArea> TickingAreaManager::remove(ServerNetworkHandler &owner,
                                                    const std::function<bool(const TickingArea &)> &match) {
    std::vector<TickingArea> removed;
    for (auto it = mAreas.begin(); it != mAreas.end();) {
        if (!match(*it)) {
            ++it;
            continue;
        }

        owner.getDimension(DimensionType::Overworld).eraseTickingArea(it->mId);
        _detach(owner, *it);
        removed.push_back(std::move(*it));
        it = mAreas.erase(it);
    }
    return removed;
}

std::vector<TickingArea> TickingAreaManager::setPreload(ServerNetworkHandler &owner,
                                                        const std::function<bool(const TickingArea &)> &match,
                                                        bool preload) {
    std::vector<TickingArea> updated;
    for (TickingArea &area: mAreas) {
        if (!match(area))
            continue;

        area.mPreload = preload;
        owner.getDimension(DimensionType::Overworld).saveTickingArea(area.mId, area.toNbt());
        updated.push_back(area);
    }
    return updated;
}

void TickingAreaManager::appendColumns(DimensionType dimension, std::vector<int64_t> &columns) const {
    for (const TickingArea &area: mAreas) {
        if (area.mDimension != dimension)
            continue;

        const std::vector<int64_t> areaColumns = area.getColumns();
        columns.insert(columns.end(), areaColumns.begin(), areaColumns.end());
    }
}

uint64_t TickingAreaManager::_loaderId(const TickingArea &area) {
    return std::hash<std::string>()(area.mId) | LOADER_ID_FLAG;
}

std::string TickingAreaManager::_randomId() {
    static std::mt19937_64 generator(std::random_device{}());
    uint64_t most = generator();
    uint64_t least = generator();
    most = (most & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    least = (least & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
    return Uuid(most, least).toString();
}

void TickingAreaManager::_attach(ServerNetworkHandler &owner, const TickingArea &area) {
    Level &level = owner.getDimension(area.mDimension);
    const uint64_t loader = _loaderId(area);

    for (const int64_t column: area.getColumns()) {
        const int32_t chunkX = (int32_t) (column >> 32);
        const int32_t chunkZ = (int32_t) (column & 0xffffffff);
        level.registerChunkLoader(loader, chunkX, chunkZ);
        if (area.mPreload)
            level.getChunk(chunkX, chunkZ);
        else
            level.requestChunkAsync(chunkX, chunkZ);
    }

    owner.markActiveColumnsDirty();
}

void TickingAreaManager::_detach(ServerNetworkHandler &owner, const TickingArea &area) {
    Level &level = owner.getDimension(area.mDimension);
    const uint64_t loader = _loaderId(area);

    for (const int64_t column: area.getColumns())
        level.unregisterChunkLoader(loader, (int32_t) (column >> 32), (int32_t) (column & 0xffffffff));

    owner.markActiveColumnsDirty();
}
