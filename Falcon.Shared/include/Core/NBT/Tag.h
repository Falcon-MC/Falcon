#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

class NbtException : public std::runtime_error {
public:
    explicit NbtException(const std::string &message) : std::runtime_error(message) {
    }
};

class Tag {
public:
    enum class Type : unsigned char {
        End = 0,
        Byte = 1,
        Short = 2,
        Int = 3,
        Long = 4,
        Float = 5,
        Double = 6,
        ByteArray = 7,
        String = 8,
        List = 9,
        Compound = 10,
        IntArray = 11,
        LongArray = 12
    };

    Tag();

    static const char *getTypeName(Type type);

    static bool isValidType(int id);

    static Tag ofByte(int8_t value);

    static Tag ofShort(int16_t value);

    static Tag ofInt(int32_t value);

    static Tag ofLong(int64_t value);

    static Tag ofFloat(float value);

    static Tag ofDouble(double value);

    static Tag ofByteArray(std::vector<int8_t> value);

    static Tag ofString(std::string value);

    static Tag ofList(Type elementType, std::vector<Tag> values = {});

    static Tag ofCompound();

    static Tag ofIntArray(std::vector<int32_t> value);

    static Tag ofLongArray(std::vector<int64_t> value);

    Type getType() const {
        return mType;
    }

    bool isCompound() const {
        return mType == Type::Compound;
    }

    bool isList() const {
        return mType == Type::List;
    }

    int8_t asByte() const;

    int16_t asShort() const;

    int32_t asInt() const;

    int64_t asLong() const;

    float asFloat() const;

    double asDouble() const;

    const std::string &asString() const;

    const std::vector<int8_t> &asByteArray() const;

    const std::vector<int32_t> &asIntArray() const;

    const std::vector<int64_t> &asLongArray() const;

    Type getListType() const;

    const std::vector<Tag> &getList() const;

    void addToList(Tag value);

    size_t size() const;

    bool isEmpty() const {
        return size() == 0;
    }

    const std::vector<std::string> &getKeys() const;

    const std::vector<Tag> &getValues() const;

    bool contains(const std::string &key) const;

    bool contains(const std::string &key, Type type) const;

    const Tag *get(const std::string &key) const;

    Tag *get(const std::string &key);

    void put(const std::string &key, Tag value);

    bool remove(const std::string &key);

    void putByte(const std::string &key, int8_t value) {
        put(key, ofByte(value));
    }

    void putShort(const std::string &key, int16_t value) {
        put(key, ofShort(value));
    }

    void putInt(const std::string &key, int32_t value) {
        put(key, ofInt(value));
    }

    void putLong(const std::string &key, int64_t value) {
        put(key, ofLong(value));
    }

    void putFloat(const std::string &key, float value) {
        put(key, ofFloat(value));
    }

    void putDouble(const std::string &key, double value) {
        put(key, ofDouble(value));
    }

    void putString(const std::string &key, std::string value) {
        put(key, ofString(std::move(value)));
    }

    void putBool(const std::string &key, bool value) {
        put(key, ofByte(value ? 1 : 0));
    }

    int8_t getByte(const std::string &key, int8_t defaultValue = 0) const;

    int16_t getShort(const std::string &key, int16_t defaultValue = 0) const;

    int32_t getInt(const std::string &key, int32_t defaultValue = 0) const;

    int64_t getLong(const std::string &key, int64_t defaultValue = 0) const;

    float getFloat(const std::string &key, float defaultValue = 0.0f) const;

    double getDouble(const std::string &key, double defaultValue = 0.0) const;

    std::string getString(const std::string &key, const std::string &defaultValue = std::string()) const;

    bool getBool(const std::string &key, bool defaultValue = false) const;

    std::string toString() const;

    bool operator==(const Tag &right) const;

    bool operator!=(const Tag &right) const {
        return !(*this == right);
    }

private:
    void _expect(Type type) const;

    Type mType;

    int8_t mByte;
    int16_t mShort;
    int32_t mInt;
    int64_t mLong;
    float mFloat;
    double mDouble;

    std::string mString;
    std::vector<int8_t> mByteArray;
    std::vector<int32_t> mIntArray;
    std::vector<int64_t> mLongArray;

    Type mListType;
    std::vector<Tag> mValues;
    std::vector<std::string> mKeys;
};
