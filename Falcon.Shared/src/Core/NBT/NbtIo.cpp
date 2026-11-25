#include "Core/NBT/NbtIo.h"

namespace {

    int16_t readShort(ReadOnlyBinaryStream &stream, NbtVariant variant) {
        return variant == NbtVariant::BigEndian ? stream.getSignedShort() : stream.getSignedLShort();
    }

    void writeShort(BinaryStream &stream, NbtVariant variant, int16_t value) {
        if (variant == NbtVariant::BigEndian)
            stream.putShort((uint16_t) value);
        else
            stream.putLShort((uint16_t) value);
    }

    int32_t readInt(ReadOnlyBinaryStream &stream, NbtVariant variant) {
        switch (variant) {
            case NbtVariant::BigEndian:
                return stream.getSignedInt();
            case NbtVariant::LittleEndian:
                return stream.getSignedLInt();
            default:
                return stream.getVarInt();
        }
    }

    void writeInt(BinaryStream &stream, NbtVariant variant, int32_t value) {
        switch (variant) {
            case NbtVariant::BigEndian:
                stream.putInt((uint32_t) value);
                break;
            case NbtVariant::LittleEndian:
                stream.putLInt((uint32_t) value);
                break;
            default:
                stream.putVarInt(value);
                break;
        }
    }

    int64_t readLong(ReadOnlyBinaryStream &stream, NbtVariant variant) {
        switch (variant) {
            case NbtVariant::BigEndian:
                return stream.getSignedLong();
            case NbtVariant::LittleEndian:
                return stream.getSignedLLong();
            default:
                return stream.getVarLong();
        }
    }

    void writeLong(BinaryStream &stream, NbtVariant variant, int64_t value) {
        switch (variant) {
            case NbtVariant::BigEndian:
                stream.putLong((uint64_t) value);
                break;
            case NbtVariant::LittleEndian:
                stream.putLLong((uint64_t) value);
                break;
            default:
                stream.putVarLong(value);
                break;
        }
    }

    float readFloat(ReadOnlyBinaryStream &stream, NbtVariant variant) {
        return variant == NbtVariant::BigEndian ? stream.getFloat() : stream.getLFloat();
    }

    void writeFloat(BinaryStream &stream, NbtVariant variant, float value) {
        if (variant == NbtVariant::BigEndian)
            stream.putFloat(value);
        else
            stream.putLFloat(value);
    }

    double readDouble(ReadOnlyBinaryStream &stream, NbtVariant variant) {
        return variant == NbtVariant::BigEndian ? stream.getDouble() : stream.getLDouble();
    }

    void writeDouble(BinaryStream &stream, NbtVariant variant, double value) {
        if (variant == NbtVariant::BigEndian)
            stream.putDouble(value);
        else
            stream.putLDouble(value);
    }

    std::string readString(ReadOnlyBinaryStream &stream, NbtVariant variant) {
        size_t length;

        switch (variant) {
            case NbtVariant::BigEndian:
                length = stream.getShort();
                break;
            case NbtVariant::LittleEndian:
                length = stream.getLShort();
                break;
            default:
                length = stream.getUnsignedVarInt();
                break;
        }

        if (length > stream.getEncodingSettings().mMaxStringLength) {
            throw BinaryDataException("Tried to read an NBT string of " + std::to_string(length) +
                                      " bytes but the maximum is " +
                                      std::to_string(stream.getEncodingSettings().mMaxStringLength));
        }

        return stream.get(length);
    }

    void writeString(BinaryStream &stream, NbtVariant variant, const std::string &value) {
        switch (variant) {
            case NbtVariant::BigEndian:
                stream.putShort((uint16_t) value.size());
                break;
            case NbtVariant::LittleEndian:
                stream.putLShort((uint16_t) value.size());
                break;
            default:
                stream.putUnsignedVarInt((uint32_t) value.size());
                break;
        }

        stream.put(value);
    }

    size_t readArrayLength(ReadOnlyBinaryStream &stream, NbtVariant variant, size_t maximum) {
        const int32_t length = readInt(stream, variant);

        if (length < 0 || (size_t) length > maximum) {
            throw BinaryDataException("Tried to read an NBT array of " + std::to_string(length) +
                                      " entries but the maximum is " + std::to_string(maximum));
        }

        return (size_t) length;
    }

}

Tag NbtIo::readTag(ReadOnlyBinaryStream &stream, NbtVariant variant, std::string *outRootName) {
    const unsigned char typeId = stream.getByte();

    if (!Tag::isValidType(typeId))
        throw BinaryDataException("Unknown NBT tag id " + std::to_string((int) typeId));

    const Tag::Type type = (Tag::Type) typeId;

    const std::string rootName = readString(stream, variant);
    if (outRootName != nullptr)
        *outRootName = rootName;

    return readValue(stream, type, variant);
}

void NbtIo::writeTag(BinaryStream &stream, const Tag &tag, NbtVariant variant, const std::string &rootName) {
    stream.putByte((unsigned char) tag.getType());
    writeString(stream, variant, rootName);
    writeValue(stream, tag, variant);
}

Tag NbtIo::readValue(ReadOnlyBinaryStream &stream, Tag::Type type, NbtVariant variant, int maxDepth) {
    if (maxDepth < 0)
        throw BinaryDataException("NBT compound is too deeply nested");

    const EncodingSettings &settings = stream.getEncodingSettings();

    switch (type) {
        case Tag::Type::End:
            return Tag();

        case Tag::Type::Byte:
            return Tag::ofByte((int8_t) stream.getByte());

        case Tag::Type::Short:
            return Tag::ofShort(readShort(stream, variant));

        case Tag::Type::Int:
            return Tag::ofInt(readInt(stream, variant));

        case Tag::Type::Long:
            return Tag::ofLong(readLong(stream, variant));

        case Tag::Type::Float:
            return Tag::ofFloat(readFloat(stream, variant));

        case Tag::Type::Double:
            return Tag::ofDouble(readDouble(stream, variant));

        case Tag::Type::String:
            return Tag::ofString(readString(stream, variant));

        case Tag::Type::ByteArray: {
            const size_t length = readArrayLength(stream, variant, settings.mMaxByteArraySize);
            const std::string bytes = stream.get(length);
            return Tag::ofByteArray(std::vector<int8_t>((const int8_t *) bytes.data(),
                                                        (const int8_t *) bytes.data() + bytes.size()));
        }

        case Tag::Type::IntArray: {
            const size_t length = readArrayLength(stream, variant, settings.mMaxListSize);

            std::vector<int32_t> values;
            values.reserve(length);

            for (size_t i = 0; i < length; i++)
                values.push_back(readInt(stream, variant));

            return Tag::ofIntArray(std::move(values));
        }

        case Tag::Type::LongArray: {
            const size_t length = readArrayLength(stream, variant, settings.mMaxListSize);

            std::vector<int64_t> values;
            values.reserve(length);

            for (size_t i = 0; i < length; i++)
                values.push_back(readLong(stream, variant));

            return Tag::ofLongArray(std::move(values));
        }

        case Tag::Type::List: {
            const unsigned char elementTypeId = stream.getByte();
            if (!Tag::isValidType(elementTypeId))
                throw BinaryDataException("Unknown NBT list type id " + std::to_string((int) elementTypeId));

            const Tag::Type elementType = (Tag::Type) elementTypeId;
            const size_t length = readArrayLength(stream, variant, settings.mMaxListSize);

            // an END element type carries no bytes, so a non-empty list of it would allocate many empty tags
            // while consuming nothing from the stream
            if (elementType == Tag::Type::End && length > 0)
                throw BinaryDataException("NBT list of END must be empty");

            std::vector<Tag> values;
            values.reserve(length);

            for (size_t i = 0; i < length; i++)
                values.push_back(readValue(stream, elementType, variant, maxDepth - 1));

            return Tag::ofList(elementType, std::move(values));
        }

        case Tag::Type::Compound: {
            Tag compound = Tag::ofCompound();

            for (;;) {
                const unsigned char entryTypeId = stream.getByte();
                if (!Tag::isValidType(entryTypeId))
                    throw BinaryDataException("Unknown NBT tag id " + std::to_string((int) entryTypeId));

                const Tag::Type entryType = (Tag::Type) entryTypeId;
                if (entryType == Tag::Type::End)
                    return compound;

                if (compound.size() >= settings.mMaxListSize)
                    throw BinaryDataException("NBT compound has too many entries");

                const std::string key = readString(stream, variant);
                compound.put(key, readValue(stream, entryType, variant, maxDepth - 1));
            }
        }

        default:
            throw BinaryDataException("Unknown NBT tag id " + std::to_string((int) type));
    }
}

void NbtIo::writeValue(BinaryStream &stream, const Tag &tag, NbtVariant variant, int maxDepth) {
    if (maxDepth < 0)
        throw NbtException("NBT compound is too deeply nested");

    switch (tag.getType()) {
        case Tag::Type::End:
            break;

        case Tag::Type::Byte:
            stream.putByte((unsigned char) tag.asByte());
            break;

        case Tag::Type::Short:
            writeShort(stream, variant, tag.asShort());
            break;

        case Tag::Type::Int:
            writeInt(stream, variant, tag.asInt());
            break;

        case Tag::Type::Long:
            writeLong(stream, variant, tag.asLong());
            break;

        case Tag::Type::Float:
            writeFloat(stream, variant, tag.asFloat());
            break;

        case Tag::Type::Double:
            writeDouble(stream, variant, tag.asDouble());
            break;

        case Tag::Type::String:
            writeString(stream, variant, tag.asString());
            break;

        case Tag::Type::ByteArray: {
            const std::vector<int8_t> &values = tag.asByteArray();
            writeInt(stream, variant, (int32_t) values.size());
            stream.put(values.data(), values.size());
            break;
        }

        case Tag::Type::IntArray: {
            const std::vector<int32_t> &values = tag.asIntArray();
            writeInt(stream, variant, (int32_t) values.size());

            for (int32_t value: values)
                writeInt(stream, variant, value);
            break;
        }

        case Tag::Type::LongArray: {
            const std::vector<int64_t> &values = tag.asLongArray();
            writeInt(stream, variant, (int32_t) values.size());

            for (int64_t value: values)
                writeLong(stream, variant, value);
            break;
        }

        case Tag::Type::List: {
            const std::vector<Tag> &values = tag.getList();
            stream.putByte((unsigned char) tag.getListType());
            writeInt(stream, variant, (int32_t) values.size());

            for (const Tag &value: values)
                writeValue(stream, value, variant, maxDepth - 1);
            break;
        }

        case Tag::Type::Compound: {
            const std::vector<std::string> &keys = tag.getKeys();
            const std::vector<Tag> &values = tag.getValues();

            for (size_t i = 0; i < keys.size(); i++) {
                stream.putByte((unsigned char) values[i].getType());
                writeString(stream, variant, keys[i]);
                writeValue(stream, values[i], variant, maxDepth - 1);
            }

            stream.putByte((unsigned char) Tag::Type::End);
            break;
        }
    }
}
