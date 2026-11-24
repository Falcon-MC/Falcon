#pragma once

#include <string>

// the dedicated server's pack optimizer, run with PackOptimizerConfigPath=<config.json> in place of the server;
// each folder directly in input_directory is a pack, written to output_directory under the same name:
//   by default its folders are baked into __brarchive/<folder>.brarchive, JSON minified
//   with "brpak" the whole pack goes into <pack>/<pack>.brpak
//   with "unpack" an optimized pack turns back into plain files
class PackOptimizer {
public:
    static constexpr const char *ARGUMENT = "PackOptimizerConfigPath=";

    struct Config {
        std::string mInputDirectory;
        std::string mOutputDirectory;
        bool mVerbose = false;
        bool mUnpack = false;
        bool mBrpak = false;
    };

    // false with the reason when the file is missing, malformed or lacks a directory
    static bool loadConfig(const std::string &path, Config &config, std::string &error);

    // the process exit code: 0, or 7 like the game for an unusable configuration or a pack that failed
    static int run(const std::string &configPath);

    static int run(const Config &config);
};
