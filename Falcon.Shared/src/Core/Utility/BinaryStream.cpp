#include "Core/Utility/BinaryStream.h"

#include <cstring>

BinaryStream::BinaryStream() {
    mBuffer.reserve(256);
}

BinaryStream::BinaryStream(std::string buffer) : ReadOnlyBinaryStream(std::move(buffer)) {
}

void BinaryStream::reset() {
    mBuffer.clear();
    mOffset = 0;
}

void BinaryStream::put(const std::string &bytes) {
    mBuffer.append(bytes);
}

void BinaryStream::put(const void *data, size_t length) {
    if (data == nullptr || length == 0)
        return;

    mBuffer.append((const char *) data, length);
}

void BinaryStream::putByte(unsigned char value) {
    mBuffer.push_back((char) value);
}

void BinaryStream::putBool(bool value) {
    putByte(value ? 1 : 0);
}

void BinaryStream::putShort(uint16_t value) {
    putByte((unsigned char) ((value >> 8) & 0xff));
    putByte((unsigned char) (value & 0xff));
}

void BinaryStream::putLShort(uint16_t value) {
    putByte((unsigned char) (value & 0xff));
    putByte((unsigned char) ((value >> 8) & 0xff));
}

void BinaryStream::putInt(uint32_t value) {
    for (int i = 3; i >= 0; i--)
        putByte((unsigned char) ((value >> (i * 8)) & 0xff));
}

void BinaryStream::putLInt(uint32_t value) {
    for (int i = 0; i < 4; i++)
        putByte((unsigned char) ((value >> (i * 8)) & 0xff));
}

void BinaryStream::putLong(uint64_t value) {
    for (int i = 7; i >= 0; i--)
        putByte((unsigned char) ((value >> (i * 8)) & 0xff));
}

void BinaryStream::putLLong(uint64_t value) {
    for (int i = 0; i < 8; i++)
        putByte((unsigned char) ((value >> (i * 8)) & 0xff));
}

void BinaryStream::putFloat(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    putInt(bits);
}

void BinaryStream::putLFloat(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    putLInt(bits);
}

void BinaryStream::putDouble(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    putLong(bits);
}

void BinaryStream::putLDouble(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    putLLong(bits);
}

void BinaryStream::putUnsignedVarInt(uint32_t value) {
    for (;;) {
        if ((value & ~0x7fu) == 0) {
            putByte((unsigned char) value);
            return;
        }

        putByte((unsigned char) ((value & 0x7f) | 0x80));
        value >>= 7;
    }
}

void BinaryStream::putVarInt(int32_t value) {
    putUnsignedVarInt((uint32_t) ((value << 1) ^ (value >> 31)));
}

void BinaryStream::putUnsignedVarLong(uint64_t value) {
    for (;;) {
        if ((value & ~0x7full) == 0) {
            putByte((unsigned char) value);
            return;
        }

        putByte((unsigned char) ((value & 0x7f) | 0x80));
        value >>= 7;
    }
}

void BinaryStream::putVarLong(int64_t value) {
    putUnsignedVarLong((uint64_t) ((value << 1) ^ (value >> 63)));
}

void BinaryStream::putString(const std::string &value) {
    putUnsignedVarInt((uint32_t) value.size());
    put(value);
}

void BinaryStream::putVector3f(const Vector3f &value) {
    putLFloat(value.x);
    putLFloat(value.y);
    putLFloat(value.z);
}

void BinaryStream::putVector2f(const Vector2f &value) {
    putLFloat(value.x);
    putLFloat(value.y);
}

void BinaryStream::putVector3i(const Vector3i &value) {
    putVarInt(value.x);
    putVarInt(value.y);
    putVarInt(value.z);
}

void BinaryStream::putBlockPosition(const Vector3i &value) {
    putVarInt(value.x);
    putVarInt(value.y);
    putVarInt(value.z);
}

void BinaryStream::putUuid(const Uuid &value) {
    putLLong(value.mostSignificantBits);
    putLLong(value.leastSignificantBits);
}

void BinaryStream::putByteArray(const std::string &value) {
    putUnsignedVarInt((uint32_t) value.size());
    put(value);
}

void BinaryStream::putArrayLength(uint32_t length) {
    putUnsignedVarInt(length);
}

void BinaryStream::putOptionalPresent(bool present) {
    putBool(present);
}
