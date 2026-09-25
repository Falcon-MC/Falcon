#pragma once

#include <string>

class NativeLibrary {
public:
    NativeLibrary() = default;

    ~NativeLibrary();

    NativeLibrary(const NativeLibrary &) = delete;

    NativeLibrary &operator=(const NativeLibrary &) = delete;

    bool open(const std::string &path, std::string &error);

    void *symbol(const char *name) const;

    void close();

    static std::string fileName(const std::string &main);

private:
    void *mHandle = nullptr;
};
