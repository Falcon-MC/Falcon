#include "Network/JsonText.h"

#include <cstdio>
#include <memory>

namespace JsonText {

    namespace {

        void skipWhitespace(const std::string &text, size_t &position) {
            while (position < text.size() && std::isspace((unsigned char) text[position]))
                ++position;
        }

        bool skipString(const std::string &text, size_t &position) {
            if (position >= text.size() || text[position] != '"')
                return false;

            ++position;

            while (position < text.size()) {
                const char character = text[position++];

                if (character == '\\') {
                    if (position >= text.size())
                        return false;

                    ++position;
                    continue;
                }

                if (character == '"')
                    return true;
            }

            return false;
        }

        bool skipValue(const std::string &text, size_t &position) {
            skipWhitespace(text, position);

            if (position >= text.size())
                return false;

            const char first = text[position];

            if (first == '"')
                return skipString(text, position);

            if (first == '{' || first == '[') {
                int depth = 0;

                while (position < text.size()) {
                    const char character = text[position];

                    if (character == '"') {
                        if (!skipString(text, position))
                            return false;

                        continue;
                    }

                    ++position;

                    if (character == '{' || character == '[') {
                        ++depth;
                    } else if (character == '}' || character == ']') {
                        --depth;

                        if (depth == 0)
                            return true;
                    }
                }

                return false;
            }

            const size_t start = position;

            while (position < text.size()) {
                const char character = text[position];

                if (character == ',' || character == '}' || character == ']' ||
                    std::isspace((unsigned char) character))
                    break;

                ++position;
            }

            return position > start;
        }

        bool equalsIgnoreCase(const std::string &left, const std::string &right) {
            if (left.size() != right.size())
                return false;

            for (size_t index = 0; index < left.size(); ++index) {
                if (std::tolower((unsigned char) left[index]) != std::tolower((unsigned char) right[index]))
                    return false;
            }

            return true;
        }

    }

    std::string quote(const std::string &value) {
        std::string out;
        out.reserve(value.size() + 2);
        out.push_back('"');

        for (char character: value) {
            switch (character) {
                case '"':
                    out += "\\\"";
                    break;

                case '\\':
                    out += "\\\\";
                    break;

                case '\n':
                    out += "\\n";
                    break;

                case '\r':
                    out += "\\r";
                    break;

                case '\t':
                    out += "\\t";
                    break;

                default:
                    if ((unsigned char) character < 0x20) {
                        char buffer[8];
                        snprintf(buffer, sizeof(buffer), "\\u%04x", (unsigned int) (unsigned char) character);
                        out += buffer;
                    } else {
                        out.push_back(character);
                    }
                    break;
            }
        }

        out.push_back('"');
        return out;
    }

    bool findMember(const std::string &objectText, const std::string &key, bool ignoreCase,
                    std::string &outRawValue) {
        size_t position = 0;
        skipWhitespace(objectText, position);

        if (position >= objectText.size() || objectText[position] != '{')
            return false;

        ++position;

        for (;;) {
            skipWhitespace(objectText, position);

            if (position >= objectText.size() || objectText[position] == '}')
                return false;

            const size_t keyStart = position;
            if (!skipString(objectText, position))
                return false;

            std::string name;
            if (!readString(objectText.substr(keyStart, position - keyStart), name))
                return false;

            skipWhitespace(objectText, position);

            if (position >= objectText.size() || objectText[position] != ':')
                return false;

            ++position;
            skipWhitespace(objectText, position);

            const size_t valueStart = position;
            if (!skipValue(objectText, position))
                return false;

            if (ignoreCase ? equalsIgnoreCase(name, key) : name == key) {
                outRawValue = objectText.substr(valueStart, position - valueStart);
                return true;
            }

            skipWhitespace(objectText, position);

            if (position < objectText.size() && objectText[position] == ',') {
                ++position;
                continue;
            }

            return false;
        }
    }

    bool splitArray(const std::string &arrayText, std::vector<std::string> &outElements) {
        outElements.clear();

        size_t position = 0;
        skipWhitespace(arrayText, position);

        if (position >= arrayText.size() || arrayText[position] != '[')
            return false;

        ++position;
        skipWhitespace(arrayText, position);

        if (position < arrayText.size() && arrayText[position] == ']')
            return true;

        for (;;) {
            skipWhitespace(arrayText, position);

            const size_t start = position;
            if (!skipValue(arrayText, position))
                return false;

            outElements.push_back(arrayText.substr(start, position - start));
            skipWhitespace(arrayText, position);

            if (position >= arrayText.size())
                return false;

            if (arrayText[position] == ']')
                return true;

            if (arrayText[position] != ',')
                return false;

            ++position;
        }
    }

    bool readString(const std::string &rawValue, std::string &outValue) {
        std::unique_ptr<json::Value> value = json::parse(rawValue);

        if (value == nullptr || !value->isString())
            return false;

        outValue = std::move(value->mString);
        return true;
    }

    const json::Value *getMember(const json::Value &object, const std::string &key, bool ignoreCase) {
        const json::Value *exact = object.get(key);

        if (exact != nullptr || !ignoreCase)
            return exact;

        for (const auto &entry: object.mObject) {
            if (equalsIgnoreCase(entry.first, key))
                return entry.second.get();
        }

        return nullptr;
    }

}
