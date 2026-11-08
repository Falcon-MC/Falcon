#pragma once

#include "World/Level/Chunk/RawPack.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// most sections hold one level (open sky, darkness underground, no block light): nibble arrays only once
// non-uniform, compact() folds them back; the section table only exists while one is allocated
class LightStorage {
public:
    static constexpr int SECTION_COUNT = 24;
    static constexpr int MIN_Y = -64;

    bool isAllocated() const {
        return mAllocated;
    }

    int get(int x, int32_t y, int z) const {
        if (!mAllocated)
            return 0;

        const int section = (y - MIN_Y) >> 4;
        if (mSections.empty())
            return mUniform[(size_t) section];

        const std::vector<uint8_t> &data = mSections[(size_t) section];
        if (data.empty())
            return mUniform[(size_t) section];

        const int index = _index(x, y, z);
        const uint8_t packed = data[(size_t) (index >> 1)];
        return (index & 1) != 0 ? packed >> 4 : packed & 0x0F;
    }

    void set(int x, int32_t y, int z, int value) {
        if (!mAllocated) {
            mUniform.fill(0);
            mAllocated = true;
        }

        const int section = (y - MIN_Y) >> 4;
        if (mSections.empty()) {
            if (value == mUniform[(size_t) section])
                return;

            mSections.resize(SECTION_COUNT);
        }

        std::vector<uint8_t> &data = mSections[(size_t) section];
        if (data.empty()) {
            const uint8_t uniform = mUniform[(size_t) section];
            if (value == uniform)
                return;

            data.assign(SECTION_BYTES, (uint8_t) (uniform | (uniform << 4)));
        }

        const int index = _index(x, y, z);
        uint8_t &packed = data[(size_t) (index >> 1)];
        if ((index & 1) != 0)
            packed = (uint8_t) ((packed & 0x0F) | ((value & 0x0F) << 4));
        else
            packed = (uint8_t) ((packed & 0xF0) | (value & 0x0F));
    }

    void compact() {
        bool kept = false;
        for (size_t section = 0; section < mSections.size(); ++section) {
            std::vector<uint8_t> &data = mSections[section];
            if (data.empty())
                continue;

            const uint8_t first = data[0];
            if ((first & 0x0F) != (first >> 4)) {
                kept = true;
                continue;
            }

            bool uniform = true;
            for (const uint8_t packed: data) {
                if (packed != first) {
                    uniform = false;
                    break;
                }
            }
            if (!uniform) {
                kept = true;
                continue;
            }

            mUniform[section] = (uint8_t) (first & 0x0F);
            std::vector<uint8_t>().swap(data);
        }

        if (!kept)
            std::vector<std::vector<uint8_t>>().swap(mSections);
    }

    void clear() {
        std::vector<std::vector<uint8_t>>().swap(mSections);
        mAllocated = false;
    }

    void packRaw(std::string &out) const {
        RawPack::put(out, mAllocated);
        RawPack::put(out, mUniform);
        RawPack::put(out, (uint8_t) (mSections.empty() ? 0 : 1));
        for (const std::vector<uint8_t> &data: mSections)
            RawPack::putVector(out, data);
    }

    bool unpackRaw(RawPack::Reader &in) {
        uint8_t hasSections = 0;
        if (!in.get(mAllocated) || !in.get(mUniform) || !in.get(hasSections))
            return false;

        mSections.clear();
        if (hasSections == 0)
            return true;

        mSections.resize(SECTION_COUNT);
        for (std::vector<uint8_t> &data: mSections) {
            if (!in.getVector(data) || (!data.empty() && data.size() != SECTION_BYTES))
                return false;
        }
        return true;
    }

private:
    static constexpr size_t SECTION_BYTES = 16 * 16 * 16 / 2;

    static int _index(int x, int32_t y, int z) {
        return (((y - MIN_Y) & 15) << 8) | (z << 4) | x;
    }

    std::vector<std::vector<uint8_t>> mSections;
    std::array<uint8_t, SECTION_COUNT> mUniform{};
    bool mAllocated = false;
};
