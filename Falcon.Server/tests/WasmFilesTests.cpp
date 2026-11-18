#include "Falcon/Plugin/LoadedPlugin.h"
#include "Falcon/Plugin/WasmHost.h"
#include "Falcon/Plugin/WasmPlugin.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"

#include <wasmtime.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // writes saved.txt when loaded, tries to leave its folder, prints a line, and writes after.txt on enable
    const char *const MODULE = R"((module
  (import "wasi_snapshot_preview1" "path_open"
    (func $open (param i32 i32 i32 i32 i32 i64 i64 i32 i32) (result i32)))
  (import "wasi_snapshot_preview1" "fd_write" (func $write (param i32 i32 i32 i32) (result i32)))
  (import "wasi_snapshot_preview1" "fd_close" (func $close (param i32) (result i32)))
  (memory (export "memory") 1)
  (data (i32.const 16) "saved.txt")
  (data (i32.const 32) "../escape.txt")
  (data (i32.const 48) "/etc/escape.txt")
  (data (i32.const 64) "kept")
  (data (i32.const 80) "hello from wasm\n")
  (data (i32.const 112) "after.txt")
  ;; path, length: a new file holding "kept", or nothing when it cannot be created
  (func $save (param $path i32) (param $length i32)
    (i32.store (i32.const 200) (i32.const 64))
    (i32.store (i32.const 204) (i32.const 4))
    (if (i32.eqz (call $open (i32.const 3) (i32.const 1) (local.get $path) (local.get $length) (i32.const 9)
                       (i64.const 66) (i64.const 66) (i32.const 0) (i32.const 300)))
      (then
        (drop (call $write (i32.load (i32.const 300)) (i32.const 200) (i32.const 1) (i32.const 304)))
        (drop (call $close (i32.load (i32.const 300)))))))
  (func (export "_initialize")
    (call $save (i32.const 16) (i32.const 9))
    (call $save (i32.const 32) (i32.const 13))
    (call $save (i32.const 48) (i32.const 15))
    (i32.store (i32.const 208) (i32.const 80))
    (i32.store (i32.const 212) (i32.const 16))
    (drop (call $write (i32.const 1) (i32.const 208) (i32.const 1) (i32.const 304))))
  (func (export "falcon:plugin/plugin@1.0.6#on-enable") (result i32)
    (call $save (i32.const 112) (i32.const 9))
    (i32.const 1)))
)";

    std::string read(const fs::path &path) {
        std::ifstream file(path, std::ios::binary);
        std::ostringstream text;
        text << file.rdbuf();
        return text.str();
    }

    void run(const fs::path &root) {
        ServerNetworkHandler owner("Wasm files", "", 1);

        LoadedPlugin plugin;
        plugin.mDescription.mName = "Files";
        plugin.mDescription.mVersion = "1.0.0";
        plugin.mDescription.mMain = "files.wasm";
        plugin.mDirectory = (root / "plugins" / "Files").string();
        plugin.mDataFolder = (root / "plugins" / "Files" / "data").string();
        fs::create_directories(plugin.mDataFolder);

        wasm_byte_vec_t wasm;
        wasmtime_error_t *error = wasmtime_wat2wasm(MODULE, std::char_traits<char>::length(MODULE), &wasm);
        require(error == nullptr, "the test module does not assemble");
        std::ofstream(fs::path(plugin.mDirectory) / plugin.mDescription.mMain, std::ios::binary)
                .write(wasm.data, (std::streamsize) wasm.size);
        wasm_byte_vec_delete(&wasm);

        require(WasmHost::load(plugin), "the plugin did not load");
        const fs::path data(plugin.mDataFolder);
        require(read(data / "saved.txt") == "kept", "the plugin could not write in its data folder");
        // the data folder is all it sees
        require(!fs::exists(root / "plugins" / "Files" / "escape.txt"), "the plugin wrote outside its data folder");
        require(!fs::exists(data / "etc" / "escape.txt") || read(data / "etc" / "escape.txt").empty(),
                "an absolute path left the data folder");

        // above its disk limit it can read but not write, and writes again once back under
        auto *wasmPlugin = static_cast<WasmPlugin *>(plugin.mInstance.get());
        require(wasmPlugin->canWriteFiles(), "the plugin starts unable to write");
        const fs::path big = data / "big.bin";
        std::ofstream(big).put('x');
        fs::resize_file(big, 65ull * 1024 * 1024);
        wasmPlugin->checkDisk(true);
        require(!wasmPlugin->canWriteFiles(), "a plugin over its disk limit can still write");
        require(plugin.mCallbacks.onEnable(plugin.mCallbacks.userData) == 1, "the plugin did not enable");
        require(!fs::exists(data / "after.txt"), "a plugin over its disk limit wrote a file");
        require(read(data / "saved.txt") == "kept", "the files of a plugin over its limit changed");

        fs::remove(big);
        wasmPlugin->checkDisk(true);
        require(wasmPlugin->canWriteFiles(), "a plugin back under its disk limit cannot write");
        plugin.mCallbacks.onEnable(plugin.mCallbacks.userData);
        require(read(data / "after.txt") == "kept", "a plugin back under its disk limit could not write");
        plugin.mInstance.reset();
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const fs::path root = fs::temp_directory_path()
                          / ("falcon-wasm-files-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        fs::create_directories(root);
        ServerPaths::setRoot(root);
        run(root);
        std::cout << "wasm file checks passed\n";
    } catch (const std::exception &error) {
        std::clog << "Wasm file regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    fs::remove_all(root, ignored);
    return result;
}
