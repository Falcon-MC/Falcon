#pragma once

#include <initializer_list>
#include <string>
#include <string_view>

namespace BlockIdentifier {
    inline bool endsWith(std::string_view identifier, std::string_view suffix) {
        if (identifier.size() < suffix.size())
            return false;

        return identifier.compare(identifier.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    inline bool startsWith(std::string_view identifier, std::string_view prefix) {
        if (identifier.size() < prefix.size())
            return false;

        return identifier.compare(0, prefix.size(), prefix) == 0;
    }

    inline bool equalsAny(std::string_view identifier, std::initializer_list<std::string_view> candidates) {
        for (const std::string_view &candidate: candidates) {
            if (identifier == candidate)
                return true;
        }

        return false;
    }

    inline bool endsWithAny(std::string_view identifier, std::initializer_list<std::string_view> suffixes) {
        for (const std::string_view &suffix: suffixes) {
            if (endsWith(identifier, suffix))
                return true;
        }

        return false;
    }
}
