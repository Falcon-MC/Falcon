#pragma once

#include <filesystem>
#include <string>

/**
 * Where the server keeps its files. The dedicated server leaves the root empty and works in the current
 * directory; a host that embeds the server points it at its own data folder instead.
 */
class ServerPaths {
public:
    static void setRoot(const std::filesystem::path &root);

    static const std::filesystem::path &getRoot();

    static std::filesystem::path resolve(const std::string &relative);

    static std::string file(const std::string &relative);
};
