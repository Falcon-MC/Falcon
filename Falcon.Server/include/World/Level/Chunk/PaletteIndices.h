#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * Per-position palette indices of one sub-chunk storage. They are held on one byte while every
 * index fits, which covers nearly all sections, and widen to two bytes on the first larger index.
 * These arrays are the largest allocation of a loaded chunk, so the narrow form halves chunk memory.
 */
class PaletteIndices {
public:
    bool empty() const {
        return mNarrow.empty() && mWide.empty();
    }

    uint16_t operator[](size_t index) const {
        return mWide.empty() ? mNarrow[index] : mWide[index];
    }

    void set(size_t index, uint16_t value) {
        if (mWide.empty() && value > UINT8_MAX)
            _widen();

        if (!mWide.empty())
            mWide[index] = value;
        else
            mNarrow[index] = (uint8_t) value;
    }

    void assign(size_t count, uint16_t value) {
        clear();
        if (value > UINT8_MAX)
            mWide.assign(count, value);
        else
            mNarrow.assign(count, (uint8_t) value);
    }

    // Releases the storage, unlike std::vector::clear().
    void clear() {
        std::vector<uint8_t>().swap(mNarrow);
        std::vector<uint16_t>().swap(mWide);
    }

private:
    void _widen() {
        mWide.assign(mNarrow.begin(), mNarrow.end());
        std::vector<uint8_t>().swap(mNarrow);
    }

    std::vector<uint8_t> mNarrow;
    std::vector<uint16_t> mWide;
};
