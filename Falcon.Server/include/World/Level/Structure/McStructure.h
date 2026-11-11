#pragma once

#include "Core/Math/Vector3i.h"
#include "Core/NBT/Tag.h"
#include "World/Level/Block/BlockState.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class Level;
class ServerNetworkHandler;

// a .mcstructure: two block layers in zyx order (z fastest), -1 a void cell left untouched on placement
class McStructure {
public:
    static constexpr int32_t VOID_INDEX = -1;
    static constexpr int32_t FORMAT_VERSION = 1;

    enum class Mirror { None, X, Z, XZ };

    struct PlaceOptions {
        // quarter turns clockwise seen from above
        int32_t mRotation = 0;
        Mirror mMirror = Mirror::None;
        bool mBlocks = true;
        bool mEntities = true;
        bool mWaterlogged = false;
        // a block that can hold water and lands in water keeps it on layer 1
        bool mKeepWorldWater = false;
        // the share of blocks kept, 1 keeps all
        float mIntegrity = 1.0f;
        uint32_t mSeed = 0;
        // cells [mFromCell, mToCell) in y, x, z order: an animated placement sets them a step at a time
        int64_t mFromCell = 0;
        int64_t mToCell = INT64_MAX;
        // called with each block before it is set: may change it, false skips the cell
        std::function<bool(const Vector3i &target, BlockState &state)> mFilter;
    };

    McStructure() = default;

    // every cell void
    explicit McStructure(const Vector3i &size);

    const Vector3i &getSize() const {
        return mSize;
    }

    bool contains(const Vector3i &local) const;

    // null for a void cell
    const BlockState *getBlock(const Vector3i &local, int32_t layer = 0) const;

    // null makes the cell void
    void setBlock(const Vector3i &local, const BlockState *state, int32_t layer = 0);

    bool isWaterlogged(const Vector3i &local) const;

    const Tag *getBlockEntity(const Vector3i &local) const;

    void setBlockEntity(const Vector3i &local, Tag data);

    const std::vector<Tag> &getEntities() const {
        return mEntities;
    }

    void addEntity(Tag entity) {
        mEntities.push_back(std::move(entity));
    }

    const Vector3i &getWorldOrigin() const {
        return mWorldOrigin;
    }

    Tag toNbt() const;

    // false for anything but a structure tag
    static bool fromNbt(const Tag &root, McStructure &out);

    // the file bytes: little endian nbt with an empty root name
    std::string serialize() const;

    static bool parse(const std::string &bytes, McStructure &out);

    // corners in any order; block actors and, unless excluded, actors inside the box come along
    static McStructure capture(ServerNetworkHandler &owner, Level &level, const Vector3i &from, const Vector3i &to,
                               bool blocks, bool entities);

    // the size once rotated
    Vector3i rotatedSize(int32_t rotation) const;

    // where a cell lands relative to the placement corner
    Vector3i transform(const Vector3i &local, const PlaceOptions &options) const;

    void place(ServerNetworkHandler &owner, Level &level, const Vector3i &at, const PlaceOptions &options) const;

private:
    size_t _index(const Vector3i &local) const;

    static bool _kept(size_t index, float integrity, uint32_t seed);

    int32_t _paletteIndex(const BlockState &state);

    Vector3i mSize;
    Vector3i mWorldOrigin;
    std::vector<BlockState> mPalette;
    std::vector<int32_t> mLayers[2];
    std::unordered_map<int32_t, Tag> mBlockEntities;
    std::vector<Tag> mEntities;
};
