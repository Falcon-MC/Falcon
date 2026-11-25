#pragma once

#include "Core/Archive/ZipArchive.h"

#include <cstdint>
#include <string>
#include <vector>

// the game's packed pack (.brpak): a ZIP of stored entries with the comment BRPAK/1
namespace BrPak {
    constexpr const char *COMMENT = "BRPAK/1";

    // an entry name the game accepts: relative, '/' separated, no '..' component
    bool isValidEntryName(const std::string &name);

    // headers as the game writes them: version 1.0, UTF-8 names, rw-rw-rw- files; times are MS-DOS local time.
    // false for an invalid or repeated name
    bool write(const std::vector<ZipEntry> &entries, uint16_t dosTime, uint16_t dosDate, std::string &out);
}
