#pragma once

#include "RakNet/RakNetDefines.h"
#include "RakNet/RakNetTypes.h"

#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace RakNet {

    class BitStream {
    public:
        BitStream();

        explicit BitStream(BitSize_t initialBytesToAllocate);

        BitStream(unsigned char *data, unsigned int lengthInBytes, bool copyData);

        void Reset();

        void ResetReadPointer() { readOffset = 0; }

        void ResetWritePointer() { numberOfBitsUsed = 0; }

        void WriteBits(const unsigned char *inByteArray, BitSize_t numberOfBitsToWrite, bool rightAlignedBits = true);

        bool ReadBits(unsigned char *inOutByteArray, BitSize_t numberOfBitsToRead, bool alignBitsToRight = true);

        void Write0();

        void Write1();

        bool ReadBit();

        void Write(const char *inputByteArray, unsigned int numberOfBytes);

        bool Read(char *outByteArray, unsigned int numberOfBytes);

        void WriteAlignedBytes(const unsigned char *inByteArray, unsigned int numberOfBytesToWrite);

        bool ReadAlignedBytes(unsigned char *inOutByteArray, unsigned int numberOfBytesToRead);

        void AlignWriteToByteBoundary();

        void AlignReadToByteBoundary();

        template<class templateType>
        void Write(templateType var);

        template<class templateType>
        bool Read(templateType &var);

        unsigned char *GetData() const { return data.empty() ? nullptr : const_cast<unsigned char *>(data.data()); }

        BitSize_t GetNumberOfBitsUsed() const { return numberOfBitsUsed; }

        BitSize_t GetNumberOfBytesUsed() const { return BITS_TO_BYTES(numberOfBitsUsed); }

        BitSize_t GetReadOffset() const { return readOffset; }

        void SetReadOffset(BitSize_t offset) { readOffset = offset; }

        void SetWriteOffset(BitSize_t offset) { numberOfBitsUsed = offset; }

        BitSize_t GetNumberOfUnreadBits() const {
            return numberOfBitsUsed > readOffset ? numberOfBitsUsed - readOffset : 0;
        }

        void IgnoreBits(BitSize_t numberOfBits) { readOffset += numberOfBits; }

        void IgnoreBytes(unsigned int numberOfBytes) { IgnoreBits(BYTES_TO_BITS(numberOfBytes)); }

        static bool DoEndianSwap();

        static void ReverseBytes(unsigned char *inByteArray, unsigned char *inOutByteArray, unsigned int length);

    private:
        void AddBitsAndReallocate(BitSize_t numberOfBitsToWrite);

        std::vector<unsigned char> data;
        BitSize_t numberOfBitsUsed;
        BitSize_t readOffset;
    };

    template<class templateType>
    void BitStream::Write(templateType var) {
        static_assert(std::is_arithmetic<templateType>::value, "Use an explicit overload for non arithmetic types");

        if (sizeof(templateType) == 1) {
            WriteBits((const unsigned char *) &var, BYTES_TO_BITS(sizeof(templateType)), true);
            return;
        }

        if (DoEndianSwap()) {
            unsigned char swapped[sizeof(templateType)];
            ReverseBytes((unsigned char *) &var, swapped, sizeof(templateType));
            WriteBits(swapped, BYTES_TO_BITS(sizeof(templateType)), true);
        } else {
            WriteBits((const unsigned char *) &var, BYTES_TO_BITS(sizeof(templateType)), true);
        }
    }

    template<class templateType>
    bool BitStream::Read(templateType &var) {
        static_assert(std::is_arithmetic<templateType>::value, "Use an explicit overload for non arithmetic types");

        if (sizeof(templateType) == 1) {
            return ReadBits((unsigned char *) &var, BYTES_TO_BITS(sizeof(templateType)), true);
        }

        if (DoEndianSwap()) {
            unsigned char swapped[sizeof(templateType)];
            if (!ReadBits(swapped, BYTES_TO_BITS(sizeof(templateType)), true))
                return false;
            ReverseBytes(swapped, (unsigned char *) &var, sizeof(templateType));
            return true;
        }

        return ReadBits((unsigned char *) &var, BYTES_TO_BITS(sizeof(templateType)), true);
    }

    template<>
    void BitStream::Write<bool>(bool var);

    template<>
    bool BitStream::Read<bool>(bool &var);

    template<>
    void BitStream::Write<uint24_t>(uint24_t var);

    template<>
    bool BitStream::Read<uint24_t>(uint24_t &var);

    template<>
    void BitStream::Write<SystemAddress>(SystemAddress var);

    template<>
    bool BitStream::Read<SystemAddress>(SystemAddress &var);

    template<>
    void BitStream::Write<RakNetGUID>(RakNetGUID var);

    template<>
    bool BitStream::Read<RakNetGUID>(RakNetGUID &var);

}
