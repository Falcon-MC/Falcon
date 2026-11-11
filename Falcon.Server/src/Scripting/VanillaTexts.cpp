#include "Scripting/VanillaTexts.h"

#include "Server/ServerPaths.h"

#include <filesystem>
#include <fstream>
#include <iterator>

namespace {
    const char *TEXTS_FILE = "texts/en_US.lang";

    std::string readFile(const std::filesystem::path &path) {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open())
            return std::string();
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
}

const VanillaTexts &VanillaTexts::getInstance() {
    static const VanillaTexts instance;
    return instance;
}

VanillaTexts::VanillaTexts() {
    const std::filesystem::path packs = ServerPaths::resolve("resource_packs");
    std::error_code error;
    // the vanilla pack first, then any pack carrying the game's file
    const std::filesystem::path vanilla = packs / "vanilla" / TEXTS_FILE;
    if (std::filesystem::exists(vanilla, error)) {
        loadFromString(readFile(vanilla));
        return;
    }
    if (!std::filesystem::is_directory(packs, error))
        return;
    for (const auto &entry: std::filesystem::directory_iterator(packs, error)) {
        const std::filesystem::path texts = entry.path() / TEXTS_FILE;
        if (entry.is_directory(error) && std::filesystem::exists(texts, error)) {
            loadFromString(readFile(texts));
            return;
        }
    }
}

void VanillaTexts::loadFromString(const std::string &contents) {
    size_t start = 0;
    while (start < contents.size()) {
        size_t end = contents.find('\n', start);
        if (end == std::string::npos)
            end = contents.size();
        std::string line = contents.substr(start, end - start);
        start = end + 1;

        // lines end in \r on some packs; a tab starts the trailing comment
        const size_t comment = line.find('\t');
        if (comment != std::string::npos)
            line.resize(comment);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (line.empty() || line.compare(0, 2, "##") == 0)
            continue;

        const size_t equals = line.find('=');
        if (equals == std::string::npos || equals == 0)
            continue;
        mTexts[line.substr(0, equals)] = line.substr(equals + 1);
    }
}

std::string VanillaTexts::translate(const std::string &key) const {
    const auto found = mTexts.find(key);
    return found == mTexts.end() ? key : found->second;
}
