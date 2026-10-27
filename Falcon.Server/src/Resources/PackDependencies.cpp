#include "Resources/PackDependencies.h"

#include <cstddef>

namespace {
    size_t findMatchingBracket(const std::string &text, size_t open) {
        int depth = 0;
        for (size_t i = open; i < text.size(); ++i) {
            if (text[i] == '[')
                depth++;
            else if (text[i] == ']') {
                depth--;
                if (depth == 0)
                    return i;
            }
        }
        return std::string::npos;
    }

    std::string extractString(const std::string &text, size_t from, size_t limit) {
        const std::string key = "\"uuid\"";
        const size_t keyPos = text.find(key, from);
        if (keyPos == std::string::npos || keyPos >= limit)
            return std::string();

        const size_t colon = text.find(':', keyPos + key.size());
        if (colon == std::string::npos || colon >= limit)
            return std::string();

        const size_t start = text.find('"', colon);
        if (start == std::string::npos || start >= limit)
            return std::string();

        const size_t end = text.find('"', start + 1);
        if (end == std::string::npos || end >= limit)
            return std::string();

        return text.substr(start + 1, end - start - 1);
    }
}

std::vector<std::string> collectPackDependencyUuids(const std::string &manifest) {
    std::vector<std::string> result;

    const std::string key = "\"dependencies\"";
    const size_t keyPos = manifest.find(key);
    if (keyPos == std::string::npos)
        return result;

    const size_t arrayStart = manifest.find('[', keyPos + key.size());
    if (arrayStart == std::string::npos)
        return result;

    const size_t arrayEnd = findMatchingBracket(manifest, arrayStart);
    if (arrayEnd == std::string::npos)
        return result;

    size_t cursor = arrayStart;
    for (;;) {
        const std::string uuid = extractString(manifest, cursor, arrayEnd);
        if (uuid.empty())
            break;

        result.push_back(uuid);

        const size_t next = manifest.find("\"uuid\"", cursor);
        if (next == std::string::npos || next >= arrayEnd)
            break;
        cursor = next + 6;
    }

    return result;
}
