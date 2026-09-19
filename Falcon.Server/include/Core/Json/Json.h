#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct JsonValue {
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
    std::string mString;
    std::vector<std::unique_ptr<JsonValue>> mArray;
    std::unordered_map<std::string, std::unique_ptr<JsonValue>> mObject;

    const JsonValue *get(const std::string &key) const;

    int32_t integer(int32_t fallback = 0) const;

    std::string string(const std::string &fallback = std::string()) const;
};

std::string escapeJson(const std::string &value);

class JsonParser {
public:
    explicit JsonParser(const std::string &source);

    std::unique_ptr<JsonValue> parse();

private:
    void skipWhitespace();

    std::unique_ptr<JsonValue> parseValue();

    std::unique_ptr<JsonValue> parseObject();

    std::unique_ptr<JsonValue> parseArray();

    bool parseString(std::string &result);

    std::unique_ptr<JsonValue> parseLiteral(const char *literal, JsonValue::Type type, bool boolean);

    std::unique_ptr<JsonValue> parseNumber();

    bool consume(char expected);

    const std::string &mSource;
    size_t mPosition = 0;
};
