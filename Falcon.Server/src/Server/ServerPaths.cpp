#include "Server/ServerPaths.h"

namespace {
    std::filesystem::path &rootPath() {
        static std::filesystem::path root;
        return root;
    }
}

void ServerPaths::setRoot(const std::filesystem::path &root) {
    rootPath() = root;
}

const std::filesystem::path &ServerPaths::getRoot() {
    return rootPath();
}

std::filesystem::path ServerPaths::resolve(const std::string &relative) {
    if (rootPath().empty())
        return std::filesystem::path(relative);

    return rootPath() / relative;
}

std::string ServerPaths::file(const std::string &relative) {
    return resolve(relative).string();
}
