#pragma once

#include "Core/Math/Vector3i.h"
#include "Core/NBT/Tag.h"
#include "Level/Dimension.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class ServerNetworkHandler;

struct TickingArea {
    std::string mId;
    std::string mName;
    DimensionType mDimension = DimensionType::Overworld;
    bool mCircle = false;
    int32_t mMinX = 0;
    int32_t mMinZ = 0;
    int32_t mMaxX = 0;
    int32_t mMaxZ = 0;
    bool mPreload = false;

    int32_t getCenterChunkX() const;

    int32_t getCenterChunkZ() const;

    int32_t getRadius() const;

    bool containsChunk(int32_t chunkX, int32_t chunkZ) const;

    bool containsBlock(const Vector3i &position) const;

    std::vector<int64_t> getColumns() const;

    Tag toNbt() const;

    static TickingArea fromNbt(const std::string &id, const Tag &data);

    static TickingArea box(const Vector3i &from, const Vector3i &to);

    static TickingArea circle(const Vector3i &center, int32_t radius);
};

class TickingAreaManager {
public:
    static constexpr size_t MAX_AREAS = 10;

    static constexpr int32_t MAX_CHUNKS = 100;

    static constexpr int32_t MAX_RADIUS = 4;

    void load(ServerNetworkHandler &owner);

    const std::vector<TickingArea> &getAreas() const {
        return mAreas;
    }

    bool hasName(const std::string &name) const;

    std::string nextDefaultName() const;

    const TickingArea &add(ServerNetworkHandler &owner, TickingArea area);

    std::vector<TickingArea> remove(ServerNetworkHandler &owner, const std::function<bool(const TickingArea &)> &match);

    std::vector<TickingArea> setPreload(ServerNetworkHandler &owner,
                                        const std::function<bool(const TickingArea &)> &match, bool preload);

    void appendColumns(DimensionType dimension, std::vector<int64_t> &columns) const;

private:
    static uint64_t _loaderId(const TickingArea &area);

    static std::string _randomId();

    void _attach(ServerNetworkHandler &owner, const TickingArea &area);

    void _detach(ServerNetworkHandler &owner, const TickingArea &area);

    std::vector<TickingArea> mAreas;
};
