#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * Per-position palette indices of one sub-chunk storage, bit-packed into 32-bit words like the
 * Bedrock storage format. Entry widths are limited to 1, 2, 4, 8 and 16 bits so a lookup is two
 * shifts and a mask; the width grows on the first index that does not fit. These arrays are the
 * largest allocation of a loaded chunk, so packing them sets most of the server's memory use. A
 * storage whose indices are all zero, such as a single-biome section, keeps no words at all.
 */
class PaletteIndices {
public:
    bool empty() const {
        return mCount == 0;
    }

    uint16_t operator[](size_t index) const {
        if (mWords.empty())
            return 0;

        const uint32_t word = mWords[index >> (5 - mWidthLog2)];
        const uint32_t shift = (uint32_t) (index & ((1u << (5 - mWidthLog2)) - 1u)) << mWidthLog2;
        return (uint16_t) ((word >> shift) & _mask(mWidthLog2));
    }

    void set(size_t index, uint16_t value) {
        if (mWords.empty()) {
            if (value == 0)
                return;

            mWords.assign(_wordCount(mCount, mWidthLog2), 0);
        }
        if (value > _mask(mWidthLog2))
            _widen(_widthFor(value));

        uint32_t &word = mWords[index >> (5 - mWidthLog2)];
        const uint32_t shift = (uint32_t) (index & ((1u << (5 - mWidthLog2)) - 1u)) << mWidthLog2;
        word = (word & ~(_mask(mWidthLog2) << shift)) | ((uint32_t) value << shift);
    }

    void assign(size_t count, uint16_t value) {
        clear();
        mCount = count;
        mWidthLog2 = _widthFor(value);
        if (value == 0)
            return;

        uint32_t word = 0;
        for (uint32_t slot = 0; slot < (1u << (5 - mWidthLog2)); ++slot)
            word |= (uint32_t) value << (slot << mWidthLog2);
        mWords.assign(_wordCount(count, mWidthLog2), word);
    }

    // Releases the storage, unlike std::vector::clear().
    void clear() {
        std::vector<uint32_t>().swap(mWords);
        mCount = 0;
        mWidthLog2 = 0;
    }

private:
    static uint32_t _mask(uint32_t widthLog2) {
        return widthLog2 >= 4 ? 0xFFFFu : (1u << (1u << widthLog2)) - 1u;
    }

    static uint32_t _widthFor(uint16_t value) {
        uint32_t widthLog2 = 0;
        while (value > _mask(widthLog2))
            ++widthLog2;
        return widthLog2;
    }

    static size_t _wordCount(size_t count, uint32_t widthLog2) {
        const size_t perWord = (size_t) 1 << (5 - widthLog2);
        return (count + perWord - 1) / perWord;
    }

    void _widen(uint32_t widthLog2) {
        PaletteIndices wider;
        wider.mCount = mCount;
        wider.mWidthLog2 = widthLog2;
        wider.mWords.assign(_wordCount(mCount, widthLog2), 0);
        for (size_t index = 0; index < mCount; ++index)
            wider.set(index, (*this)[index]);
        *this = std::move(wider);
    }

    std::vector<uint32_t> mWords;
    size_t mCount = 0;
    uint32_t mWidthLog2 = 0;
};
