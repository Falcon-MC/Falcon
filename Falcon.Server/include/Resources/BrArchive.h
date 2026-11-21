#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// the game's folder archive (.brarchive): the files of one folder, without its subfolders
//   u64 magic, u32 entry count, u32 version 1
//   per entry, 256 bytes: u8 name length, 247 bytes of name padded with zeros, u32 data offset, u32 data size
//   then the data of every entry, offsets counted from the end of the entry table
namespace BrArchive {
    constexpr uint64_t MAGIC = 0x267052A0B125277Dull;
    constexpr uint32_t VERSION = 1;
    // a longer name brings the game down; refused here
    constexpr size_t MAX_NAME_LENGTH = 246;

    struct Entry {
        std::string mName;
        std::string mData;
    };

    // entries sorted by name, byte by byte, like the game; false for a name longer than MAX_NAME_LENGTH
    bool write(std::vector<Entry> entries, std::string &out);

    bool read(const std::string &archive, std::vector<Entry> &out);
}
