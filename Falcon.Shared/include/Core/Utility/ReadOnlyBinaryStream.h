#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Core/Utility/BinaryDataException.h"
#include "Core/Utility/EncodingSettings.h"
#include "Core/Utility/UUID.h"

#include <cstdint>
#include <string>

class ReadOnlyBinaryStream {
public:
    ReadOnlyBinaryStream();

    explicit ReadOnlyBinaryStream(std::string buffer);

    virtual ~ReadOnlyBinaryStream() = default;

    void setEncodingSettings(const EncodingSettings &settings) {
        mEncodingSettings = settings;
    }

    const EncodingSettings &getEncodingSettings() const {
        return mEncodingSettings;
    }

    const std::string &getBuffer() const {
        return mBuffer;
    }

    size_t getOffset() const {
        return mOffset;
    }

    void setOffset(size_t offset) {
        mOffset = offset;
    }

    void rewind() {
        mOffset = 0;
    }

    size_t getRemainingLength() const {
        return mBuffer.size() - mOffset;
    }

    bool feof() const {
        return mOffset >= mBuffer.size();
    }

    std::string get(size_t length);

    std::string getRemaining();

    unsigned char getByte();

    signed char getSignedByte();

    bool getBool();

    uint16_t getShort();

    int16_t getSignedShort();

    uint16_t getLShort();

    int16_t getSignedLShort();

    uint32_t getInt();

    int32_t getSignedInt();

    uint32_t getLInt();

    int32_t getSignedLInt();

    uint64_t getLong();

    int64_t getSignedLong();

    uint64_t getLLong();

    int64_t getSignedLLong();

    float getFloat();

    float getLFloat();

    double getDouble();

    double getLDouble();

    uint32_t getUnsignedVarInt();

    int32_t getVarInt();

    uint64_t getUnsignedVarLong();

    int64_t getVarLong();

    std::string getString();

    Vector3f getVector3f();

    Vector2f getVector2f();

    Vector3i getVector3i();

    Vector3i getBlockPosition();

    Uuid getUuid();

    std::string getByteArray();

    uint32_t getArrayLength();

    bool getOptionalPresent();

protected:
    void _readBytes(void *outData, size_t length);

    std::string mBuffer;
    size_t mOffset;
    EncodingSettings mEncodingSettings;
};
