#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * Light levels of a chunk column, kept per sub-chunk section. Most sections hold a single level (open
 * sky above the terrain, darkness below it, no block light), so a section only allocates its nibble
 * array once it stops being uniform, and compact() folds uniform arrays back into one value.
 */
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

    // Releases section arrays that hold a single level again, typically right after a full recompute.
    void compact() {
        for (size_t section = 0; section < mSections.size(); ++section) {
            std::vector<uint8_t> &data = mSections[section];
            if (data.empty())
                continue;

            const uint8_t first = data[0];
            if ((first & 0x0F) != (first >> 4))
                continue;

            bool uniform = true;
            for (const uint8_t packed: data) {
                if (packed != first) {
                    uniform = false;
                    break;
                }
            }
            if (!uniform)
                continue;

            mUniform[section] = (uint8_t) (first & 0x0F);
            std::vector<uint8_t>().swap(data);
        }
    }

    void clear() {
        for (std::vector<uint8_t> &data: mSections)
            std::vector<uint8_t>().swap(data);
        mAllocated = false;
    }

private:
    static constexpr size_t SECTION_BYTES = 16 * 16 * 16 / 2;

    static int _index(int x, int32_t y, int z) {
        return (((y - MIN_Y) & 15) << 8) | (z << 4) | x;
    }

    std::array<std::vector<uint8_t>, SECTION_COUNT> mSections;
    std::array<uint8_t, SECTION_COUNT> mUniform{};
    bool mAllocated = false;
};
