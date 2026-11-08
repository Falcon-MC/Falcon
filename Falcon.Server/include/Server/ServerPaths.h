#pragma once

#include <filesystem>
#include <string>

// empty root for the dedicated server (current directory); an embedder points it at its data folder
class ServerPaths {
public:
    static void setRoot(const std::filesystem::path &root);

    static const std::filesystem::path &getRoot();

    static std::filesystem::path resolve(const std::string &relative);

    static std::string file(const std::string &relative);
};
