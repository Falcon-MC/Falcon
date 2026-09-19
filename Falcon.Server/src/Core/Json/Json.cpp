#include "Core/Json/Json.h"

#include <cctype>
#include <cstdlib>
#include <utility>

const JsonValue *JsonValue::get(const std::string &key) const {
    const auto it = mObject.find(key);
    return it == mObject.end() ? nullptr : it->second.get();
}

int32_t JsonValue::integer(int32_t fallback) const {
    return mType == Type::Number ? (int32_t) mNumber : fallback;
}

std::string JsonValue::string(const std::string &fallback) const {
    return mType == Type::String ? mString : fallback;
}

JsonParser::JsonParser(const std::string &source) : mSource(source) {
}

std::unique_ptr<JsonValue> JsonParser::parse() {
    skipWhitespace();
    std::unique_ptr<JsonValue> result = parseValue();
    skipWhitespace();
    return result != nullptr && mPosition == mSource.size() ? std::move(result) : nullptr;
}

void JsonParser::skipWhitespace() {
    while (mPosition < mSource.size() && std::isspace((unsigned char) mSource[mPosition]))
        ++mPosition;
}

std::unique_ptr<JsonValue> JsonParser::parseValue() {
    skipWhitespace();
    if (mPosition >= mSource.size())
        return nullptr;

    switch (mSource[mPosition]) {
        case '{': return parseObject();
        case '[': return parseArray();
        case '"': {
            std::string value;
            if (!parseString(value))
                return nullptr;
            auto result = std::make_unique<JsonValue>();
            result->mType = JsonValue::Type::String;
            result->mString = std::move(value);
            return result;
        }
        case 't': return parseLiteral("true", JsonValue::Type::Boolean, true);
        case 'f': return parseLiteral("false", JsonValue::Type::Boolean, false);
        case 'n': return parseLiteral("null", JsonValue::Type::Null, false);
        default: return parseNumber();
    }
}

std::unique_ptr<JsonValue> JsonParser::parseObject() {
    ++mPosition;
    auto result = std::make_unique<JsonValue>();
    result->mType = JsonValue::Type::Object;
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
        std::unique_ptr<JsonValue> value = parseValue();
        if (value == nullptr)
            return nullptr;
        result->mObject[std::move(key)] = std::move(value);
        skipWhitespace();
        if (consume('}'))
            return result;
        if (!consume(','))
            return nullptr;
        skipWhitespace();
    }
    return nullptr;
}

std::unique_ptr<JsonValue> JsonParser::parseArray() {
    ++mPosition;
    auto result = std::make_unique<JsonValue>();
    result->mType = JsonValue::Type::Array;
    skipWhitespace();
    if (consume(']'))
        return result;

    while (mPosition < mSource.size()) {
        std::unique_ptr<JsonValue> value = parseValue();
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

bool JsonParser::parseString(std::string &result) {
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
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                case 'u':
                    if (mPosition + 4 > mSource.size())
                        return false;
                    mPosition += 4;
                    result.push_back('?');
                    break;
                default: return false;
            }
        } else {
            if ((unsigned char) value < 0x20)
                return false;
            result.push_back(value);
        }
    }
    return false;
}

std::unique_ptr<JsonValue> JsonParser::parseLiteral(const char *literal, JsonValue::Type type, bool boolean) {
    const std::string value(literal);
    if (mSource.compare(mPosition, value.size(), value) != 0)
        return nullptr;
    mPosition += value.size();
    auto result = std::make_unique<JsonValue>();
    result->mType = type;
    result->mBoolean = boolean;
    return result;
}

std::unique_ptr<JsonValue> JsonParser::parseNumber() {
    const size_t start = mPosition;
    if (mPosition < mSource.size() && (mSource[mPosition] == '-' || mSource[mPosition] == '+'))
        ++mPosition;
    while (mPosition < mSource.size() && std::isdigit((unsigned char) mSource[mPosition]))
        ++mPosition;
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

    auto result = std::make_unique<JsonValue>();
    result->mType = JsonValue::Type::Number;
    result->mNumber = value;
    return result;
}

bool JsonParser::consume(char expected) {
    if (mPosition >= mSource.size() || mSource[mPosition] != expected)
        return false;
    ++mPosition;
    return true;
}
