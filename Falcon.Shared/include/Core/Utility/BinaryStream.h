#pragma once

#include "Core/Utility/ReadOnlyBinaryStream.h"

class BinaryStream : public ReadOnlyBinaryStream {
public:
    BinaryStream();

    explicit BinaryStream(std::string buffer);

    void reset();

    size_t size() const {
        return mBuffer.size();
    }

    void put(const std::string &bytes);

    void put(const void *data, size_t length);

    void putByte(unsigned char value);

    void putBool(bool value);

    void putShort(uint16_t value);

    void putLShort(uint16_t value);

    void putInt(uint32_t value);

    void putLInt(uint32_t value);

    void putLong(uint64_t value);

    void putLLong(uint64_t value);

    void putFloat(float value);

    void putLFloat(float value);

    void putDouble(double value);

    void putLDouble(double value);

    void putUnsignedVarInt(uint32_t value);

    void putVarInt(int32_t value);

    void putUnsignedVarLong(uint64_t value);

    void putVarLong(int64_t value);

    void putString(const std::string &value);

    void putVector3f(const Vector3f &value);

    void putVector2f(const Vector2f &value);

    void putVector3i(const Vector3i &value);

    void putBlockPosition(const Vector3i &value);

    void putUuid(const Uuid &value);

    void putByteArray(const std::string &value);

    void putArrayLength(uint32_t length);

    void putOptionalPresent(bool present);
};
