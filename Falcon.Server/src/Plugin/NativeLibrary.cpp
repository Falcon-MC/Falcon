#include "Plugin/NativeLibrary.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

NativeLibrary::~NativeLibrary() {
    close();
}

bool NativeLibrary::open(const std::string &path, std::string &error) {
    close();

#if defined(_WIN32)
    mHandle = (void *) LoadLibraryA(path.c_str());
    if (mHandle == nullptr) {
        error = "LoadLibrary failed with error " + std::to_string(GetLastError());
        return false;
    }
#else
    mHandle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (mHandle == nullptr) {
        const char *reason = dlerror();
        error = reason == nullptr ? "dlopen failed" : reason;
        return false;
    }
#endif
    return true;
}

void *NativeLibrary::symbol(const char *name) const {
    if (mHandle == nullptr)
        return nullptr;

#if defined(_WIN32)
    return (void *) GetProcAddress((HMODULE) mHandle, name);
#else
    return dlsym(mHandle, name);
#endif
}

void NativeLibrary::close() {
    if (mHandle == nullptr)
        return;

#if defined(_WIN32)
    FreeLibrary((HMODULE) mHandle);
#else
    dlclose(mHandle);
#endif
    mHandle = nullptr;
}

void NativeLibrary::detach() {
    mHandle = nullptr;
}

std::string NativeLibrary::fileName(const std::string &main) {
#if defined(_WIN32)
    return main + ".dll";
#elif defined(__APPLE__)
    return "lib" + main + ".dylib";
#else
    return "lib" + main + ".so";
#endif
}
