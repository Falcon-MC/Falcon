#include "RakNet/BitStream.h"

namespace RakNet {

    BitStream::BitStream() : numberOfBitsUsed(0), readOffset(0) {
        data.reserve(256);
    }

    BitStream::BitStream(BitSize_t initialBytesToAllocate) : numberOfBitsUsed(0), readOffset(0) {
        data.reserve(initialBytesToAllocate);
    }

    BitStream::BitStream(unsigned char *inData, unsigned int lengthInBytes, bool copyData)
            : numberOfBitsUsed(BYTES_TO_BITS(lengthInBytes)), readOffset(0) {
        (void) copyData;
        data.assign(inData, inData + lengthInBytes);
    }

    void BitStream::Reset() {
        data.clear();
        numberOfBitsUsed = 0;
        readOffset = 0;
    }

    bool BitStream::DoEndianSwap() {
        const uint16_t probe = 1;
        return *(const unsigned char *) &probe == 1;
    }

    void BitStream::ReverseBytes(unsigned char *inByteArray, unsigned char *inOutByteArray, unsigned int length) {
        for (unsigned int i = 0; i < length; i++)
            inOutByteArray[i] = inByteArray[length - i - 1];
    }

    void BitStream::AddBitsAndReallocate(BitSize_t numberOfBitsToWrite) {
        const size_t newSizeInBytes = BITS_TO_BYTES(numberOfBitsUsed + numberOfBitsToWrite);
        if (newSizeInBytes > data.size())
            data.resize(newSizeInBytes, 0);
    }

    void BitStream::WriteBits(const unsigned char *inByteArray, BitSize_t numberOfBitsToWrite, bool rightAlignedBits) {
        if (numberOfBitsToWrite == 0)
            return;

        AddBitsAndReallocate(numberOfBitsToWrite);

        BitSize_t offset = 0;
        const BitSize_t numberOfBitsUsedMod8 = numberOfBitsUsed & 7;

        while (numberOfBitsToWrite > 0) {
            unsigned char dataByte = inByteArray[offset];

            if (numberOfBitsToWrite < 8 && rightAlignedBits)
                dataByte = (unsigned char) (dataByte << (8 - numberOfBitsToWrite));

            if ((numberOfBitsUsed & 7) == 0) {
                data[numberOfBitsUsed >> 3] = dataByte;
            } else {
                const BitSize_t mod8 = numberOfBitsUsed & 7;
                data[numberOfBitsUsed >> 3] |= (unsigned char) (dataByte >> mod8);
                if (8 - mod8 < numberOfBitsToWrite)
                    data[(numberOfBitsUsed >> 3) + 1] = (unsigned char) (dataByte << (8 - mod8));
            }

            if (numberOfBitsToWrite >= 8) {
                numberOfBitsUsed += 8;
                numberOfBitsToWrite -= 8;
            } else {
                numberOfBitsUsed += numberOfBitsToWrite;
                numberOfBitsToWrite = 0;
            }
            offset++;
        }

        (void) numberOfBitsUsedMod8;
    }

    bool BitStream::ReadBits(unsigned char *inOutByteArray, BitSize_t numberOfBitsToRead, bool alignBitsToRight) {
        if (numberOfBitsToRead == 0)
            return false;
        if (readOffset + numberOfBitsToRead > numberOfBitsUsed)
            return false;

        BitSize_t offset = 0;
        memset(inOutByteArray, 0, BITS_TO_BYTES(numberOfBitsToRead));

        while (numberOfBitsToRead > 0) {
            const BitSize_t readOffsetMod8 = readOffset & 7;

            inOutByteArray[offset] |= (unsigned char) (data[readOffset >> 3] << readOffsetMod8);

            if (readOffsetMod8 > 0 && numberOfBitsToRead > 8 - readOffsetMod8)
                inOutByteArray[offset] |= (unsigned char) (data[(readOffset >> 3) + 1] >> (8 - readOffsetMod8));

            if (numberOfBitsToRead >= 8) {
                numberOfBitsToRead -= 8;
                readOffset += 8;
                offset++;
            } else {
                const int neg = (int) numberOfBitsToRead - 8;
                if (neg < 0) {
                    if (alignBitsToRight)
                        inOutByteArray[offset] >>= -neg;
                    readOffset += 8 + neg;
                } else {
                    readOffset += 8;
                }
                offset++;
                numberOfBitsToRead = 0;
            }
        }

        return true;
    }

    void BitStream::Write0() {
        AddBitsAndReallocate(1);
        if ((numberOfBitsUsed & 7) == 0)
            data[numberOfBitsUsed >> 3] = 0;
        numberOfBitsUsed++;
    }

    void BitStream::Write1() {
        AddBitsAndReallocate(1);
        const BitSize_t mod8 = numberOfBitsUsed & 7;
        if (mod8 == 0)
            data[numberOfBitsUsed >> 3] = 0x80;
        else
            data[numberOfBitsUsed >> 3] |= (unsigned char) (0x80 >> mod8);
        numberOfBitsUsed++;
    }

    bool BitStream::ReadBit() {
        if (readOffset >= numberOfBitsUsed)
            return false;
        const bool result = (data[readOffset >> 3] & (0x80 >> (readOffset & 7))) != 0;
        readOffset++;
        return result;
    }

    void BitStream::Write(const char *inputByteArray, unsigned int numberOfBytes) {
        if (numberOfBytes == 0)
            return;
        WriteBits((const unsigned char *) inputByteArray, BYTES_TO_BITS(numberOfBytes), true);
    }

    bool BitStream::Read(char *outByteArray, unsigned int numberOfBytes) {
        if (numberOfBytes == 0)
            return true;
        return ReadBits((unsigned char *) outByteArray, BYTES_TO_BITS(numberOfBytes), true);
    }

    void BitStream::AlignWriteToByteBoundary() {
        if (numberOfBitsUsed & 7)
            numberOfBitsUsed += 8 - (numberOfBitsUsed & 7);
        AddBitsAndReallocate(0);
    }

    void BitStream::AlignReadToByteBoundary() {
        if (readOffset & 7)
            readOffset += 8 - (readOffset & 7);
    }

    void BitStream::WriteAlignedBytes(const unsigned char *inByteArray, unsigned int numberOfBytesToWrite) {
        AlignWriteToByteBoundary();
        Write((const char *) inByteArray, numberOfBytesToWrite);
    }

    bool BitStream::ReadAlignedBytes(unsigned char *inOutByteArray, unsigned int numberOfBytesToRead) {
        AlignReadToByteBoundary();
        return Read((char *) inOutByteArray, numberOfBytesToRead);
    }

    template<>
    void BitStream::Write<bool>(bool var) {
        if (var)
            Write1();
        else
            Write0();
    }

    template<>
    bool BitStream::Read<bool>(bool &var) {
        if (readOffset >= numberOfBitsUsed)
            return false;
        var = ReadBit();
        return true;
    }

    template<>
    void BitStream::Write<uint24_t>(uint24_t var) {
        AlignWriteToByteBoundary();

        unsigned char bytes[3];
        bytes[0] = (unsigned char) (var.val & 0xff);
        bytes[1] = (unsigned char) ((var.val >> 8) & 0xff);
        bytes[2] = (unsigned char) ((var.val >> 16) & 0xff);

        WriteAlignedBytes(bytes, 3);
    }

    template<>
    bool BitStream::Read<uint24_t>(uint24_t &var) {
        AlignReadToByteBoundary();

        unsigned char bytes[3];
        if (!ReadAlignedBytes(bytes, 3))
            return false;

        var = uint24_t((uint32_t) bytes[0] | ((uint32_t) bytes[1] << 8) | ((uint32_t) bytes[2] << 16));
        return true;
    }

    template<>
    void BitStream::Write<SystemAddress>(SystemAddress var) {
        var.Serialize(this);
    }

    template<>
    bool BitStream::Read<SystemAddress>(SystemAddress &var) {
        return var.Deserialize(this);
    }

    template<>
    void BitStream::Write<RakNetGUID>(RakNetGUID var) {
        Write(var.g);
    }

    template<>
    bool BitStream::Read<RakNetGUID>(RakNetGUID &var) {
        return Read(var.g);
    }

}
