#pragma once

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace json {
    struct Value {
        enum class Type {
            Null,
            Boolean,
            Number,
            String,
            Array,
            Object
        };

        Type mType = Type::Null;
        bool mBoolean = false;
        double mNumber = 0.0;
        bool mInteger = false;
        std::string mString;
        std::vector<std::unique_ptr<Value>> mArray;
        std::unordered_map<std::string, std::unique_ptr<Value>> mObject;
        std::vector<std::string> mKeys;

        static std::unique_ptr<Value> ofNull() {
            return std::make_unique<Value>();
        }

        static std::unique_ptr<Value> ofBoolean(bool value) {
            auto result = std::make_unique<Value>();
            result->mType = Type::Boolean;
            result->mBoolean = value;
            return result;
        }

        static std::unique_ptr<Value> ofInteger(int64_t value) {
            auto result = std::make_unique<Value>();
            result->mType = Type::Number;
            result->mNumber = (double) value;
            result->mInteger = true;
            return result;
        }

        static std::unique_ptr<Value> ofNumber(double value) {
            auto result = std::make_unique<Value>();
            result->mType = Type::Number;
            result->mNumber = value;
            return result;
        }

        static std::unique_ptr<Value> ofString(std::string value) {
            auto result = std::make_unique<Value>();
            result->mType = Type::String;
            result->mString = std::move(value);
            return result;
        }

        static std::unique_ptr<Value> ofArray() {
            auto result = std::make_unique<Value>();
            result->mType = Type::Array;
            return result;
        }

        static std::unique_ptr<Value> ofObject() {
            auto result = std::make_unique<Value>();
            result->mType = Type::Object;
            return result;
        }

        void set(const std::string &key, std::unique_ptr<Value> value) {
            if (mObject.find(key) == mObject.end())
                mKeys.push_back(key);
            mObject[key] = std::move(value);
        }

        void push(std::unique_ptr<Value> value) {
            mArray.push_back(std::move(value));
        }

        std::unique_ptr<Value> clone() const {
            auto result = std::make_unique<Value>();
            result->mType = mType;
            result->mBoolean = mBoolean;
            result->mNumber = mNumber;
            result->mInteger = mInteger;
            result->mString = mString;
            for (const std::unique_ptr<Value> &element: mArray)
                result->mArray.push_back(element->clone());
            for (const std::string &key: mKeys)
                result->set(key, mObject.at(key)->clone());
            return result;
        }

        bool isObject() const {
            return mType == Type::Object;
        }

        bool isArray() const {
            return mType == Type::Array;
        }

        bool isString() const {
            return mType == Type::String;
        }

        bool isNumber() const {
            return mType == Type::Number;
        }

        const Value *get(const std::string &key) const {
            const auto it = mObject.find(key);
            return it == mObject.end() ? nullptr : it->second.get();
        }

        int32_t integer(int32_t fallback = 0) const {
            return mType == Type::Number ? (int32_t) mNumber : fallback;
        }

        double number(double fallback = 0.0) const {
            return mType == Type::Number ? mNumber : fallback;
        }

        bool boolean(bool fallback = false) const {
            return mType == Type::Boolean ? mBoolean : fallback;
        }

        std::string string(const std::string &fallback = std::string()) const {
            return mType == Type::String ? mString : fallback;
        }
    };

    class Parser {
    public:
        // parsing and destroying a nested value both recurse once per level, so the depth must stay well
        // below what the smallest thread stack can hold
        static constexpr size_t MAX_DEPTH = 128;

        explicit Parser(const std::string &source) : mSource(source) {
        }

        std::unique_ptr<Value> parse() {
            skipWhitespace();
            std::unique_ptr<Value> result = parseValue();
            skipWhitespace();
            return result != nullptr && mPosition == mSource.size() ? std::move(result) : nullptr;
        }

    private:
        void skipWhitespace() {
            while (mPosition < mSource.size() && std::isspace((unsigned char) mSource[mPosition]))
                ++mPosition;
        }

        std::unique_ptr<Value> parseValue() {
            skipWhitespace();
            if (mPosition >= mSource.size())
                return nullptr;

            switch (mSource[mPosition]) {
                case '{':
                case '[': {
                    if (mDepth >= MAX_DEPTH)
                        return nullptr;
                    ++mDepth;
                    std::unique_ptr<Value> result = mSource[mPosition] == '{' ? parseObject() : parseArray();
                    --mDepth;
                    return result;
                }
                case '"': {
                    std::string value;
                    if (!parseString(value))
                        return nullptr;
                    auto result = std::make_unique<Value>();
                    result->mType = Value::Type::String;
                    result->mString = std::move(value);
                    return result;
                }
                case 't':
                    return parseLiteral("true", Value::Type::Boolean, true);
                case 'f':
                    return parseLiteral("false", Value::Type::Boolean, false);
                case 'n':
                    return parseLiteral("null", Value::Type::Null, false);
                default:
                    return parseNumber();
            }
        }

        std::unique_ptr<Value> parseObject() {
            ++mPosition;
            auto result = std::make_unique<Value>();
            result->mType = Value::Type::Object;
            skipWhitespace();
            if (consume('}'))
                return result;

            while (mPosition < mSource.size()) {
                std::string key;
                if (!parseString(key))
                    return nullptr;
                skipWhitespace();
                if (!consume(':'))
                    return nullptr;
                std::unique_ptr<Value> value = parseValue();
                if (value == nullptr)
                    return nullptr;
                result->set(key, std::move(value));
                skipWhitespace();
                if (consume('}'))
                    return result;
                if (!consume(','))
                    return nullptr;
                skipWhitespace();
            }
            return nullptr;
        }

        std::unique_ptr<Value> parseArray() {
            ++mPosition;
            auto result = std::make_unique<Value>();
            result->mType = Value::Type::Array;
            skipWhitespace();
            if (consume(']'))
                return result;

            while (mPosition < mSource.size()) {
                std::unique_ptr<Value> value = parseValue();
                if (value == nullptr)
                    return nullptr;
                result->mArray.push_back(std::move(value));
                skipWhitespace();
                if (consume(']'))
                    return result;
                if (!consume(','))
                    return nullptr;
                skipWhitespace();
            }
            return nullptr;
        }

        bool parseString(std::string &result) {
            if (!consume('"'))
                return false;
            while (mPosition < mSource.size()) {
                const char value = mSource[mPosition++];
                if (value == '"')
                    return true;
                if (value == '\\') {
                    if (mPosition >= mSource.size())
                        return false;
                    const char escaped = mSource[mPosition++];
                    switch (escaped) {
                        case '"':
                            result.push_back('"');
                            break;
                        case '\\':
                            result.push_back('\\');
                            break;
                        case '/':
                            result.push_back('/');
                            break;
                        case 'b':
                            result.push_back('\b');
                            break;
                        case 'f':
                            result.push_back('\f');
                            break;
                        case 'n':
                            result.push_back('\n');
                            break;
                        case 'r':
                            result.push_back('\r');
                            break;
                        case 't':
                            result.push_back('\t');
                            break;
                        case 'u': {
                            uint32_t codePoint = 0;
                            if (!parseHex(codePoint))
                                return false;
                            if (codePoint >= 0xD800 && codePoint <= 0xDBFF && mPosition + 1 < mSource.size()
                                && mSource[mPosition] == '\\' && mSource[mPosition + 1] == 'u') {
                                mPosition += 2;
                                uint32_t low = 0;
                                if (!parseHex(low))
                                    return false;
                                codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                            }
                            appendUtf8(result, codePoint);
                            break;
                        }
                        default:
                            return false;
                    }
                } else {
                    if ((unsigned char) value < 0x20)
                        return false;
                    result.push_back(value);
                }
            }
            return false;
        }

        bool parseHex(uint32_t &outValue) {
            if (mPosition + 4 > mSource.size())
                return false;

            outValue = 0;
            for (int i = 0; i < 4; i++) {
                const char digit = mSource[mPosition++];
                outValue <<= 4;
                if (digit >= '0' && digit <= '9')
                    outValue |= (uint32_t) (digit - '0');
                else if (digit >= 'a' && digit <= 'f')
                    outValue |= (uint32_t) (digit - 'a' + 10);
                else if (digit >= 'A' && digit <= 'F')
                    outValue |= (uint32_t) (digit - 'A' + 10);
                else
                    return false;
            }
            return true;
        }

        static void appendUtf8(std::string &result, uint32_t codePoint) {
            if (codePoint < 0x80) {
                result.push_back((char) codePoint);
            } else if (codePoint < 0x800) {
                result.push_back((char) (0xC0 | (codePoint >> 6)));
                result.push_back((char) (0x80 | (codePoint & 0x3F)));
            } else if (codePoint < 0x10000) {
                result.push_back((char) (0xE0 | (codePoint >> 12)));
                result.push_back((char) (0x80 | ((codePoint >> 6) & 0x3F)));
                result.push_back((char) (0x80 | (codePoint & 0x3F)));
            } else {
                result.push_back((char) (0xF0 | (codePoint >> 18)));
                result.push_back((char) (0x80 | ((codePoint >> 12) & 0x3F)));
                result.push_back((char) (0x80 | ((codePoint >> 6) & 0x3F)));
                result.push_back((char) (0x80 | (codePoint & 0x3F)));
            }
        }

        std::unique_ptr<Value> parseLiteral(const char *literal, Value::Type type, bool boolean) {
            const std::string value(literal);
            if (mSource.compare(mPosition, value.size(), value) != 0)
                return nullptr;
            mPosition += value.size();
            auto result = std::make_unique<Value>();
            result->mType = type;
            result->mBoolean = boolean;
            return result;
        }

        std::unique_ptr<Value> parseNumber() {
            const size_t start = mPosition;
            bool integer = true;
            if (mPosition < mSource.size() && (mSource[mPosition] == '-' || mSource[mPosition] == '+'))
                ++mPosition;
            while (mPosition < mSource.size() && std::isdigit((unsigned char) mSource[mPosition]))
                ++mPosition;
            if (mPosition < mSource.size() && (mSource[mPosition] == '.' || mSource[mPosition] == 'e'
                                               || mSource[mPosition] == 'E'))
                integer = false;
            if (mPosition < mSource.size() && mSource[mPosition] == '.') {
                ++mPosition;
                while (mPosition < mSource.size() && std::isdigit((unsigned char) mSource[mPosition]))
                    ++mPosition;
            }
            if (mPosition < mSource.size() && (mSource[mPosition] == 'e' || mSource[mPosition] == 'E')) {
                ++mPosition;
                if (mPosition < mSource.size() && (mSource[mPosition] == '+' || mSource[mPosition] == '-'))
                    ++mPosition;
                while (mPosition < mSource.size() && std::isdigit((unsigned char) mSource[mPosition]))
                    ++mPosition;
            }
            if (start == mPosition)
                return nullptr;

            const std::string number = mSource.substr(start, mPosition - start);
            char *end = nullptr;
            const double value = std::strtod(number.c_str(), &end);
            if (end == number.c_str() || *end != '\0')
                return nullptr;

            auto result = std::make_unique<Value>();
            result->mType = Value::Type::Number;
            result->mNumber = value;
            result->mInteger = integer;
            return result;
        }

        bool consume(char expected) {
            if (mPosition >= mSource.size() || mSource[mPosition] != expected)
                return false;
            ++mPosition;
            return true;
        }

        const std::string &mSource;
        size_t mPosition = 0;
        size_t mDepth = 0;
    };

    inline std::unique_ptr<Value> parse(const std::string &source) {
        Parser parser(source);
        return parser.parse();
    }

    inline std::string escape(const std::string &value) {
        std::string escaped;
        escaped.reserve(value.size());

        for (const char character: value) {
            if (character == '"' || character == '\\')
                escaped.push_back('\\');
            escaped.push_back(character);
        }

        return escaped;
    }
}
