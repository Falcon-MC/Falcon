#include "Core/NBT/Tag.h"

#include <cstdio>
#include <cstring>

Tag::Tag()
        : mType(Type::End), mByte(0), mShort(0), mInt(0), mLong(0), mFloat(0.0f), mDouble(0.0),
          mListType(Type::End) {
}

const char *Tag::getTypeName(Type type) {
    switch (type) {
        case Type::Byte:
            return "BYTE";
        case Type::Short:
            return "SHORT";
        case Type::Int:
            return "INT";
        case Type::Long:
            return "LONG";
        case Type::Float:
            return "FLOAT";
        case Type::Double:
            return "DOUBLE";
        case Type::ByteArray:
            return "BYTE_ARRAY";
        case Type::String:
            return "STRING";
        case Type::List:
            return "LIST";
        case Type::Compound:
            return "COMPOUND";
        case Type::IntArray:
            return "INT_ARRAY";
        case Type::LongArray:
            return "LONG_ARRAY";
        default:
            return "END";
    }
}

bool Tag::isValidType(int id) {
    return id >= (int) Type::End && id <= (int) Type::LongArray;
}

Tag Tag::ofByte(int8_t value) {
    Tag tag;
    tag.mType = Type::Byte;
    tag.mByte = value;
    return tag;
}

Tag Tag::ofShort(int16_t value) {
    Tag tag;
    tag.mType = Type::Short;
    tag.mShort = value;
    return tag;
}

Tag Tag::ofInt(int32_t value) {
    Tag tag;
    tag.mType = Type::Int;
    tag.mInt = value;
    return tag;
}

Tag Tag::ofLong(int64_t value) {
    Tag tag;
    tag.mType = Type::Long;
    tag.mLong = value;
    return tag;
}

Tag Tag::ofFloat(float value) {
    Tag tag;
    tag.mType = Type::Float;
    tag.mFloat = value;
    return tag;
}

Tag Tag::ofDouble(double value) {
    Tag tag;
    tag.mType = Type::Double;
    tag.mDouble = value;
    return tag;
}

Tag Tag::ofByteArray(std::vector<int8_t> value) {
    Tag tag;
    tag.mType = Type::ByteArray;
    tag.mByteArray = std::move(value);
    return tag;
}

Tag Tag::ofString(std::string value) {
    Tag tag;
    tag.mType = Type::String;
    tag.mString = std::move(value);
    return tag;
}

Tag Tag::ofList(Type elementType, std::vector<Tag> values) {
    Tag tag;
    tag.mType = Type::List;
    tag.mListType = elementType;
    tag.mValues = std::move(values);

    for (const Tag &value: tag.mValues) {
        if (value.mType != elementType)
            throw NbtException("List element type mismatch");
    }

    return tag;
}

Tag Tag::ofCompound() {
    Tag tag;
    tag.mType = Type::Compound;
    return tag;
}

Tag Tag::ofIntArray(std::vector<int32_t> value) {
    Tag tag;
    tag.mType = Type::IntArray;
    tag.mIntArray = std::move(value);
    return tag;
}

Tag Tag::ofLongArray(std::vector<int64_t> value) {
    Tag tag;
    tag.mType = Type::LongArray;
    tag.mLongArray = std::move(value);
    return tag;
}

void Tag::_expect(Type type) const {
    if (mType != type) {
        throw NbtException(std::string("Expected tag of type ") + getTypeName(type) + " but got " +
                           getTypeName(mType));
    }
}

int8_t Tag::asByte() const {
    _expect(Type::Byte);
    return mByte;
}

int16_t Tag::asShort() const {
    _expect(Type::Short);
    return mShort;
}

int32_t Tag::asInt() const {
    _expect(Type::Int);
    return mInt;
}

int64_t Tag::asLong() const {
    _expect(Type::Long);
    return mLong;
}

float Tag::asFloat() const {
    _expect(Type::Float);
    return mFloat;
}

double Tag::asDouble() const {
    _expect(Type::Double);
    return mDouble;
}

const std::string &Tag::asString() const {
    _expect(Type::String);
    return mString;
}

const std::vector<int8_t> &Tag::asByteArray() const {
    _expect(Type::ByteArray);
    return mByteArray;
}

const std::vector<int32_t> &Tag::asIntArray() const {
    _expect(Type::IntArray);
    return mIntArray;
}

const std::vector<int64_t> &Tag::asLongArray() const {
    _expect(Type::LongArray);
    return mLongArray;
}

Tag::Type Tag::getListType() const {
    _expect(Type::List);
    return mListType;
}

const std::vector<Tag> &Tag::getList() const {
    _expect(Type::List);
    return mValues;
}

void Tag::addToList(Tag value) {
    _expect(Type::List);

    if (mValues.empty() && mListType == Type::End)
        mListType = value.mType;

    if (value.mType != mListType)
        throw NbtException("List element type mismatch");

    mValues.push_back(std::move(value));
}

size_t Tag::size() const {
    if (mType == Type::List)
        return mValues.size();

    _expect(Type::Compound);
    return mKeys.size();
}

const std::vector<std::string> &Tag::getKeys() const {
    _expect(Type::Compound);
    return mKeys;
}

const std::vector<Tag> &Tag::getValues() const {
    _expect(Type::Compound);
    return mValues;
}

const Tag *Tag::get(const std::string &key) const {
    _expect(Type::Compound);

    for (size_t i = 0; i < mKeys.size(); i++) {
        if (mKeys[i] == key)
            return &mValues[i];
    }

    return nullptr;
}

Tag *Tag::get(const std::string &key) {
    return const_cast<Tag *>(((const Tag *) this)->get(key));
}

bool Tag::contains(const std::string &key) const {
    return get(key) != nullptr;
}

bool Tag::contains(const std::string &key, Type type) const {
    const Tag *tag = get(key);
    return tag != nullptr && tag->mType == type;
}

void Tag::put(const std::string &key, Tag value) {
    _expect(Type::Compound);

    if (value.mType == Type::End)
        throw NbtException("Cannot store an END tag in a compound");

    for (size_t i = 0; i < mKeys.size(); i++) {
        if (mKeys[i] == key) {
            mValues[i] = std::move(value);
            return;
        }
    }

    mKeys.push_back(key);
    mValues.push_back(std::move(value));
}

bool Tag::remove(const std::string &key) {
    _expect(Type::Compound);

    for (size_t i = 0; i < mKeys.size(); i++) {
        if (mKeys[i] == key) {
            mKeys.erase(mKeys.begin() + i);
            mValues.erase(mValues.begin() + i);
            return true;
        }
    }

    return false;
}

int8_t Tag::getByte(const std::string &key, int8_t defaultValue) const {
    const Tag *tag = get(key);
    return tag != nullptr && tag->mType == Type::Byte ? tag->mByte : defaultValue;
}

int16_t Tag::getShort(const std::string &key, int16_t defaultValue) const {
    const Tag *tag = get(key);
    return tag != nullptr && tag->mType == Type::Short ? tag->mShort : defaultValue;
}

int32_t Tag::getInt(const std::string &key, int32_t defaultValue) const {
    const Tag *tag = get(key);
    return tag != nullptr && tag->mType == Type::Int ? tag->mInt : defaultValue;
}

int64_t Tag::getLong(const std::string &key, int64_t defaultValue) const {
    const Tag *tag = get(key);
    return tag != nullptr && tag->mType == Type::Long ? tag->mLong : defaultValue;
}

float Tag::getFloat(const std::string &key, float defaultValue) const {
    const Tag *tag = get(key);
    return tag != nullptr && tag->mType == Type::Float ? tag->mFloat : defaultValue;
}

double Tag::getDouble(const std::string &key, double defaultValue) const {
    const Tag *tag = get(key);
    return tag != nullptr && tag->mType == Type::Double ? tag->mDouble : defaultValue;
}

std::string Tag::getString(const std::string &key, const std::string &defaultValue) const {
    const Tag *tag = get(key);
    return tag != nullptr && tag->mType == Type::String ? tag->mString : defaultValue;
}

bool Tag::getBool(const std::string &key, bool defaultValue) const {
    const Tag *tag = get(key);
    return tag != nullptr && tag->mType == Type::Byte ? tag->mByte != 0 : defaultValue;
}

bool Tag::operator==(const Tag &right) const {
    if (mType != right.mType)
        return false;

    switch (mType) {
        case Type::End:
            return true;
        case Type::Byte:
            return mByte == right.mByte;
        case Type::Short:
            return mShort == right.mShort;
        case Type::Int:
            return mInt == right.mInt;
        case Type::Long:
            return mLong == right.mLong;
        case Type::Float:
            return memcmp(&mFloat, &right.mFloat, sizeof(mFloat)) == 0;
        case Type::Double:
            return memcmp(&mDouble, &right.mDouble, sizeof(mDouble)) == 0;
        case Type::ByteArray:
            return mByteArray == right.mByteArray;
        case Type::String:
            return mString == right.mString;
        case Type::IntArray:
            return mIntArray == right.mIntArray;
        case Type::LongArray:
            return mLongArray == right.mLongArray;
        case Type::List:
            return mListType == right.mListType && mValues == right.mValues;
        case Type::Compound:
            return mKeys == right.mKeys && mValues == right.mValues;
        default:
            return false;
    }
}

std::string Tag::toString() const {
    char buffer[64];

    switch (mType) {
        case Type::Byte:
            snprintf(buffer, sizeof(buffer), "%db", (int) mByte);
            return buffer;
        case Type::Short:
            snprintf(buffer, sizeof(buffer), "%ds", (int) mShort);
            return buffer;
        case Type::Int:
            snprintf(buffer, sizeof(buffer), "%d", mInt);
            return buffer;
        case Type::Long:
            snprintf(buffer, sizeof(buffer), "%lldl", (long long) mLong);
            return buffer;
        case Type::Float:
            snprintf(buffer, sizeof(buffer), "%gf", (double) mFloat);
            return buffer;
        case Type::Double:
            snprintf(buffer, sizeof(buffer), "%gd", mDouble);
            return buffer;
        case Type::String:
            return "\"" + mString + "\"";

        case Type::ByteArray: {
            std::string result = "[B;";
            for (size_t i = 0; i < mByteArray.size(); i++) {
                snprintf(buffer, sizeof(buffer), "%s%d", i > 0 ? "," : "", (int) mByteArray[i]);
                result += buffer;
            }
            return result + "]";
        }

        case Type::IntArray: {
            std::string result = "[I;";
            for (size_t i = 0; i < mIntArray.size(); i++) {
                snprintf(buffer, sizeof(buffer), "%s%d", i > 0 ? "," : "", mIntArray[i]);
                result += buffer;
            }
            return result + "]";
        }

        case Type::LongArray: {
            std::string result = "[L;";
            for (size_t i = 0; i < mLongArray.size(); i++) {
                snprintf(buffer, sizeof(buffer), "%s%lld", i > 0 ? "," : "", (long long) mLongArray[i]);
                result += buffer;
            }
            return result + "]";
        }

        case Type::List: {
            std::string result = "[";
            for (size_t i = 0; i < mValues.size(); i++) {
                if (i > 0)
                    result += ",";
                result += mValues[i].toString();
            }
            return result + "]";
        }

        case Type::Compound: {
            std::string result = "{";
            for (size_t i = 0; i < mKeys.size(); i++) {
                if (i > 0)
                    result += ",";
                result += mKeys[i] + ":" + mValues[i].toString();
            }
            return result + "}";
        }

        default:
            return "END";
    }
}
