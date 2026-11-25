#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// the JSON reader and compact writer the pack optimizer uses, matched to the game's output byte for byte:
// comments allowed, no trailing commas, numbers parsed and printed the way the game does, duplicate keys kept
namespace PackJson {
    struct Value {
        enum class Type : uint8_t { Null, False, True, Object, Array, String, Int, Uint, Int64, Uint64, Double };

        Type mType = Type::Null;
        std::string mString;
        int64_t mInteger = 0;
        uint64_t mUnsigned = 0;
        double mDouble = 0.0;
        std::vector<std::pair<std::string, Value>> mMembers;
        std::vector<Value> mElements;

        // the first member of that name, as the game looks it up; null without one
        Value *find(std::string_view name);

        const Value *find(std::string_view name) const;
    };

    // a leading UTF-8 byte order mark is skipped; false on any syntax error
    bool parse(std::string_view text, Value &out);

    // false for a number the game refuses to write (infinity)
    bool write(const Value &value, std::string &out);

    // parse then write: false when the game would keep the file as it is
    bool minify(std::string_view text, std::string &out);

    // a finite double as the game prints it: not always the shortest digits
    std::string formatDouble(double value);
}
