#include "Core/Utility/ReadOnlyBinaryStream.h"

#include <cstring>

ReadOnlyBinaryStream::ReadOnlyBinaryStream() : mOffset(0) {
}

ReadOnlyBinaryStream::ReadOnlyBinaryStream(std::string buffer) : mBuffer(std::move(buffer)), mOffset(0) {
}

void ReadOnlyBinaryStream::_readBytes(void *outData, size_t length) {
    if (mOffset + length > mBuffer.size()) {
        throw BinaryDataException("Not enough bytes left in buffer: need " + std::to_string(length) +
                                  ", have " + std::to_string(getRemainingLength()));
    }

    if (length > 0)
        memcpy(outData, mBuffer.data() + mOffset, length);

    mOffset += length;
}

std::string ReadOnlyBinaryStream::get(size_t length) {
    if (length > getRemainingLength()) {
        throw BinaryDataException("Not enough bytes left in buffer: need " + std::to_string(length) +
                                  ", have " + std::to_string(getRemainingLength()));
    }

    std::string result;
    result.resize(length);

    if (length > 0)
        _readBytes(&result[0], length);

    return result;
}

std::string ReadOnlyBinaryStream::getRemaining() {
    return get(getRemainingLength());
}

unsigned char ReadOnlyBinaryStream::getByte() {
    unsigned char value;
    _readBytes(&value, sizeof(value));
    return value;
}

signed char ReadOnlyBinaryStream::getSignedByte() {
    return (signed char) getByte();
}

bool ReadOnlyBinaryStream::getBool() {
    return getByte() != 0;
}

uint16_t ReadOnlyBinaryStream::getShort() {
    unsigned char bytes[2];
    _readBytes(bytes, sizeof(bytes));
    return (uint16_t) ((bytes[0] << 8) | bytes[1]);
}

int16_t ReadOnlyBinaryStream::getSignedShort() {
    return (int16_t) getShort();
}

uint16_t ReadOnlyBinaryStream::getLShort() {
    unsigned char bytes[2];
    _readBytes(bytes, sizeof(bytes));
    return (uint16_t) (bytes[0] | (bytes[1] << 8));
}

int16_t ReadOnlyBinaryStream::getSignedLShort() {
    return (int16_t) getLShort();
}

uint32_t ReadOnlyBinaryStream::getInt() {
    unsigned char bytes[4];
    _readBytes(bytes, sizeof(bytes));

    uint32_t value = 0;
    for (int i = 0; i < 4; i++)
        value = (value << 8) | bytes[i];

    return value;
}

int32_t ReadOnlyBinaryStream::getSignedInt() {
    return (int32_t) getInt();
}

uint32_t ReadOnlyBinaryStream::getLInt() {
    unsigned char bytes[4];
    _readBytes(bytes, sizeof(bytes));

    uint32_t value = 0;
    for (int i = 0; i < 4; i++)
        value |= (uint32_t) bytes[i] << (i * 8);

    return value;
}

int32_t ReadOnlyBinaryStream::getSignedLInt() {
    return (int32_t) getLInt();
}

uint64_t ReadOnlyBinaryStream::getLong() {
    unsigned char bytes[8];
    _readBytes(bytes, sizeof(bytes));

    uint64_t value = 0;
    for (int i = 0; i < 8; i++)
        value = (value << 8) | bytes[i];

    return value;
}

int64_t ReadOnlyBinaryStream::getSignedLong() {
    return (int64_t) getLong();
}

uint64_t ReadOnlyBinaryStream::getLLong() {
    unsigned char bytes[8];
    _readBytes(bytes, sizeof(bytes));

    uint64_t value = 0;
    for (int i = 0; i < 8; i++)
        value |= (uint64_t) bytes[i] << (i * 8);

    return value;
}

int64_t ReadOnlyBinaryStream::getSignedLLong() {
    return (int64_t) getLLong();
}

float ReadOnlyBinaryStream::getFloat() {
    const uint32_t bits = getInt();

    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

float ReadOnlyBinaryStream::getLFloat() {
    const uint32_t bits = getLInt();

    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

double ReadOnlyBinaryStream::getDouble() {
    const uint64_t bits = getLong();

    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

double ReadOnlyBinaryStream::getLDouble() {
    const uint64_t bits = getLLong();

    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

uint32_t ReadOnlyBinaryStream::getUnsignedVarInt() {
    uint32_t value = 0;

    for (int shift = 0; shift < 35; shift += 7) {
        const unsigned char byte = getByte();
        value |= (uint32_t) (byte & 0x7f) << shift;

        if ((byte & 0x80) == 0)
            return value;
    }

    throw BinaryDataException("VarInt did not terminate after 5 bytes");
}

int32_t ReadOnlyBinaryStream::getVarInt() {
    const uint32_t value = getUnsignedVarInt();
    return (int32_t) ((value >> 1) ^ (~(value & 1) + 1));
}

uint64_t ReadOnlyBinaryStream::getUnsignedVarLong() {
    uint64_t value = 0;

    for (int shift = 0; shift < 70; shift += 7) {
        const unsigned char byte = getByte();
        value |= (uint64_t) (byte & 0x7f) << shift;

        if ((byte & 0x80) == 0)
            return value;
    }

    throw BinaryDataException("VarLong did not terminate after 10 bytes");
}

int64_t ReadOnlyBinaryStream::getVarLong() {
    const uint64_t value = getUnsignedVarLong();
    return (int64_t) ((value >> 1) ^ (~(value & 1) + 1));
}

std::string ReadOnlyBinaryStream::getString() {
    const uint32_t length = getUnsignedVarInt();

    if (length > mEncodingSettings.mMaxStringLength) {
        throw BinaryDataException("Tried to read a string of " + std::to_string(length) +
                                  " bytes but the maximum is " + std::to_string(mEncodingSettings.mMaxStringLength));
    }

    return get(length);
}

Vector3f ReadOnlyBinaryStream::getVector3f() {
    const float x = getLFloat();
    const float y = getLFloat();
    const float z = getLFloat();
    return Vector3f(x, y, z);
}

Vector2f ReadOnlyBinaryStream::getVector2f() {
    const float x = getLFloat();
    const float y = getLFloat();
    return Vector2f(x, y);
}

Vector3i ReadOnlyBinaryStream::getVector3i() {
    const int32_t x = getVarInt();
    const int32_t y = getVarInt();
    const int32_t z = getVarInt();
    return Vector3i(x, y, z);
}

Vector3i ReadOnlyBinaryStream::getBlockPosition() {
    const int32_t x = getVarInt();
    const int32_t y = getVarInt();
    const int32_t z = getVarInt();
    return Vector3i(x, y, z);
}

Uuid ReadOnlyBinaryStream::getUuid() {
    const uint64_t most = getLLong();
    const uint64_t least = getLLong();
    return Uuid(most, least);
}

std::string ReadOnlyBinaryStream::getByteArray() {
    const uint32_t length = getUnsignedVarInt();

    if (length > mEncodingSettings.mMaxByteArraySize) {
        throw BinaryDataException("Tried to read a byte array of " + std::to_string(length) +
                                  " bytes but the maximum is " +
                                  std::to_string(mEncodingSettings.mMaxByteArraySize));
    }

    return get(length);
}

uint32_t ReadOnlyBinaryStream::getArrayLength() {
    const uint32_t length = getUnsignedVarInt();

    if (length > mEncodingSettings.mMaxListSize) {
        throw BinaryDataException("Tried to read a list of " + std::to_string(length) +
                                  " entries but the maximum is " + std::to_string(mEncodingSettings.mMaxListSize));
    }

    return length;
}

bool ReadOnlyBinaryStream::getOptionalPresent() {
    return getBool();
}
