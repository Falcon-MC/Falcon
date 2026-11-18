#include "Resources/PackOptimizer.h"

#include "Core/Debug/BedrockLog.h"
#include "Resources/BrArchive.h"
#include "Resources/BrPak.h"
#include "Resources/PackJson.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <vector>

namespace fs = std::filesystem;

namespace {
    constexpr int EXIT_FAILED = 7;

    const char *const MANIFEST = "manifest.json";
    const char *const ARCHIVE_FOLDER = "__brarchive";
    const char *const ARCHIVE_EXTENSION = ".brarchive";
    const char *const BRPAK_EXTENSION = ".brpak";
    const char *const SUBPACKS = "subpacks";
    const char *const STAGING_FOLDER = ".pack_optimizer";
    // a baked pack, then one stored in a .brpak
    const char *const BAKED_VERSION = "0.1.0";
    const char *const BRPAK_VERSION = "1.0.0";
    // the game refuses a JSON file past 128 MiB and keeps it as it is
    constexpr uintmax_t MAX_JSON_SIZE = 0x8000000;

    enum class Mode { Bake, Brpak, Unpack };

    struct Stats {
        std::string mPackUuid;
        int mFilesProcessed = 0;
        int mFilesJsonMinified = 0;
        int mFilesJsonParseError = 0;
        int mFilesNonJson = 0;
        int mArchivesWritten = 0;
        int mSubpacksProcessed = 0;
    };

    struct Context {
        const PackOptimizer::Config &mConfig;
        Mode mMode;
        Stats mStats;
        bool mFailed = false;

        void verbose(const std::string &line) const {
            if (mConfig.mVerbose)
                std::cout << line << '\n';
        }
    };

    std::string text(const fs::path &path) {
        return path.generic_string();
    }

    bool readFile(const fs::path &path, std::string &out) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return false;
        out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        return !file.bad();
    }

    bool writeFile(const fs::path &path, const std::string &data) {
        std::error_code error;
        fs::create_directories(path.parent_path(), error);
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
            return false;
        file.write(data.data(), (std::streamsize) data.size());
        return file.good();
    }

    char upper(char c) {
        return c >= 'a' && c <= 'z' ? (char) (c - 'a' + 'A') : c;
    }

    // the order the game meets the entries in on Windows: names compared without case
    bool listedBefore(const fs::path &left, const fs::path &right) {
        const std::string a = left.filename().string();
        const std::string b = right.filename().string();
        const size_t length = std::min(a.size(), b.size());
        for (size_t index = 0; index < length; ++index) {
            const auto x = (unsigned char) upper(a[index]);
            const auto y = (unsigned char) upper(b[index]);
            if (x != y)
                return x < y;
        }
        return a.size() < b.size();
    }

    void listDirectory(const fs::path &directory, std::vector<fs::path> &files, std::vector<fs::path> &folders) {
        std::error_code error;
        for (fs::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
            std::error_code typeError;
            if (it->is_directory(typeError))
                folders.push_back(it->path());
            else if (it->is_regular_file(typeError))
                files.push_back(it->path());
        }
        std::sort(files.begin(), files.end(), listedBefore);
        std::sort(folders.begin(), folders.end(), listedBefore);
    }

    std::string extensionOf(const std::string &fileName) {
        const size_t dot = fileName.rfind('.');
        return dot == std::string::npos ? std::string() : fileName.substr(dot + 1);
    }

    // files the game reads straight from the disk: they stay beside the archive too
    bool keepsLooseCopy(const std::string &path) {
        static const char *const PARTS[] = {"loot_tables/", "loot_tables\\", "materials/",    "materials\\",
                                            "languages.json", "blocks.json", "flipbook_textures.json",
                                            "terrain_texture.json", "biomes_client.json", "manifest.json",
                                            "contents.json", "sounds.json", "splashes.json"};
        for (const char *part: PARTS) {
            if (path.find(part) != std::string::npos)
                return true;
        }
        return false;
    }

    // stored as they are; any other file but JSON stays on the disk
    bool isArchivedAsIs(const std::string &extension) {
        return extension == "png" || extension == "tga" || extension == "js" || extension == "mcfunction";
    }

    // the header gets the optimization version, kept in place if the key is there already; false to copy it as is
    bool rewriteManifest(const std::string &source, const char *version, std::string &out, std::string *uuid) {
        PackJson::Value document;
        if (source.size() > MAX_JSON_SIZE || !PackJson::parse(source, document)
            || document.mType != PackJson::Value::Type::Object)
            return false;
        PackJson::Value *header = document.find("header");
        if (header == nullptr || header->mType != PackJson::Value::Type::Object)
            return false;
        if (uuid != nullptr && uuid->empty()) {
            const PackJson::Value *id = header->find("uuid");
            if (id != nullptr && id->mType == PackJson::Value::Type::String)
                *uuid = id->mString;
        }
        PackJson::Value tag;
        tag.mType = PackJson::Value::Type::String;
        tag.mString = version;
        if (PackJson::Value *existing = header->find("pack_optimization_version"))
            *existing = std::move(tag);
        else
            header->mMembers.emplace_back("pack_optimization_version", std::move(tag));
        out.clear();
        return PackJson::write(document, out);
    }

    void copyFile(Context &context, const fs::path &from, const fs::path &to, bool rootManifest) {
        std::string data;
        bool written = false;
        if (readFile(from, data)) {
            std::string rewritten;
            if (rootManifest && rewriteManifest(data, BAKED_VERSION, rewritten, &context.mStats.mPackUuid))
                data = std::move(rewritten);
            written = writeFile(to, data);
        }
        if (written) {
            context.verbose("copy " + text(from) + " to " + text(to));
            return;
        }
        context.verbose("unable to copy file from " + text(from) + " to " + text(to));
        context.mFailed = true;
    }

    struct FolderBake {
        const fs::path &mArchiveRoot;
        std::set<std::string> mArchives;
    };

    void writeArchive(Context &context, const fs::path &archiveRoot, const std::string &relative,
                      std::vector<BrArchive::Entry> entries) {
        const fs::path path = archiveRoot / fs::path(relative + ARCHIVE_EXTENSION);
        std::string archive;
        if (!BrArchive::write(std::move(entries), archive) || !writeFile(path, archive)) {
            context.verbose("failed to write archive " + text(path));
            context.mFailed = true;
            return;
        }
        context.verbose("write archive " + text(path));
        ++context.mStats.mArchivesWritten;
    }

    // the files of one folder: archived in bake mode, all written out to be packed in brpak mode
    void bakeFolder(Context &context, FolderBake &bake, const fs::path &source, const fs::path &output,
                    const std::string &relative, const std::vector<fs::path> &files) {
        const fs::path relativePath(relative);
        context.verbose("bake folder " + text(source) + " to " + text(output) + ", archive path "
                        + text(relativePath.has_parent_path() ? bake.mArchiveRoot / relativePath.parent_path()
                                                              : bake.mArchiveRoot));
        std::vector<BrArchive::Entry> entries;
        for (const fs::path &file: files) {
            const std::string name = file.filename().string();
            const fs::path target = output / name;
            std::string data;
            if (!readFile(file, data)) {
                context.verbose("unable to read file data: " + text(file));
                context.mFailed = true;
                continue;
            }
            ++context.mStats.mFilesProcessed;

            const std::string extension = extensionOf(name);
            std::string payload;
            bool archived = false;
            if (!data.empty() && isArchivedAsIs(extension)) {
                payload = data;
                archived = true;
            } else if (!data.empty() && extension == "json") {
                if (data.size() <= MAX_JSON_SIZE && PackJson::minify(data, payload)) {
                    archived = true;
                    ++context.mStats.mFilesJsonMinified;
                } else {
                    ++context.mStats.mFilesJsonParseError;
                }
            }
            if (extension != "json")
                ++context.mStats.mFilesNonJson;

            BrArchive::Entry entry;
            entry.mName = name;
            if (!archived) {
                if (context.mMode == Mode::Bake)
                    context.verbose("add archive entry " + text(file) + " with size 0 vs "
                                    + std::to_string(data.size()));
                if (writeFile(target, data)) {
                    context.verbose("copy " + text(file) + " to " + text(target));
                } else {
                    context.verbose("unable to write file from " + text(file) + " to " + text(target));
                    context.mFailed = true;
                }
                entries.push_back(std::move(entry));
                continue;
            }

            if (context.mMode == Mode::Bake)
                context.verbose("add archive entry " + text(file) + " with size " + std::to_string(payload.size())
                                + " vs " + std::to_string(data.size()));
            if (context.mMode == Mode::Brpak || keepsLooseCopy(text(file))) {
                if (writeFile(target, payload)) {
                    context.verbose("wrote optimized/upgraded file " + text(file) + " to " + text(target));
                } else {
                    context.verbose("unable to write file from " + text(file) + " to " + text(target));
                    context.mFailed = true;
                }
            }
            entry.mData = std::move(payload);
            entries.push_back(std::move(entry));
        }

        if (context.mMode != Mode::Bake)
            return;
        bake.mArchives.insert(relative);
        writeArchive(context, bake.mArchiveRoot, relative, std::move(entries));
    }

    // children first, as the game walks them; an archive folder left by an earlier bake is not content
    void bakeTree(Context &context, FolderBake &bake, const fs::path &source, const fs::path &output,
                  const std::string &relative) {
        std::vector<fs::path> files;
        std::vector<fs::path> folders;
        listDirectory(source, files, folders);
        for (const fs::path &folder: folders) {
            const std::string name = folder.filename().string();
            if (name == ARCHIVE_FOLDER)
                continue;
            bakeTree(context, bake, folder, output / name, relative + "/" + name);
        }
        if (!files.empty())
            bakeFolder(context, bake, source, output, relative, files);
    }

    void bakePack(Context &context, const fs::path &source, const fs::path &output) {
        const fs::path archiveRoot = output / ARCHIVE_FOLDER;
        context.verbose(std::string(context.mMode == Mode::Bake ? "bake" : "optimize") + " pack " + text(source)
                        + " to " + text(output) + ", archive root " + text(archiveRoot));

        std::vector<fs::path> files;
        std::vector<fs::path> folders;
        listDirectory(source, files, folders);
        for (const fs::path &file: files) {
            const std::string name = file.filename().string();
            copyFile(context, file, output / name, name == MANIFEST);
        }

        FolderBake bake{archiveRoot, {}};
        std::vector<fs::path> subpacks;
        for (const fs::path &folder: folders) {
            const std::string name = folder.filename().string();
            if (name == ARCHIVE_FOLDER)
                continue;
            if (name == SUBPACKS) {
                std::vector<fs::path> ignored;
                listDirectory(folder, ignored, subpacks);
                continue;
            }
            bakeTree(context, bake, folder, output / name, name);
        }

        // every folder above an archive gets one, empty when it holds no file
        if (context.mMode == Mode::Bake) {
            std::set<std::string> parents;
            for (const std::string &relative: bake.mArchives) {
                for (fs::path parent = fs::path(relative).parent_path(); !parent.empty();
                     parent = parent.parent_path()) {
                    if (bake.mArchives.count(parent.generic_string()) == 0)
                        parents.insert(parent.generic_string());
                }
            }
            for (const std::string &relative: parents)
                writeArchive(context, archiveRoot, relative, {});
        }

        // each folder in subpacks is a pack of its own; files beside them are dropped
        for (const fs::path &subpack: subpacks) {
            const std::string name = subpack.filename().string();
            if (name == ARCHIVE_FOLDER)
                continue;
            ++context.mStats.mSubpacksProcessed;
            bakePack(context, subpack, output / SUBPACKS / name);
        }
    }

    void collectFiles(const fs::path &directory, std::vector<fs::path> &out) {
        std::vector<fs::path> files;
        std::vector<fs::path> folders;
        listDirectory(directory, files, folders);
        std::vector<fs::path> entries = files;
        entries.insert(entries.end(), folders.begin(), folders.end());
        std::sort(entries.begin(), entries.end(), listedBefore);
        for (const fs::path &entry: entries) {
            std::error_code error;
            if (fs::is_directory(entry, error))
                collectFiles(entry, out);
            else
                out.push_back(entry);
        }
    }

    void dosTimestamp(uint16_t &time, uint16_t &date) {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        time = (uint16_t) ((local.tm_hour << 11) | (local.tm_min << 5) | (local.tm_sec / 2));
        date = (uint16_t) (((local.tm_year - 80) << 9) | ((local.tm_mon + 1) << 5) | local.tm_mday);
    }

    // the staged pack becomes <pack>/<pack>.brpak and the staged files go
    void packBrpak(Context &context, const fs::path &output) {
        const fs::path manifest = output / MANIFEST;
        std::string data;
        std::string rewritten;
        if (readFile(manifest, data) && rewriteManifest(data, BRPAK_VERSION, rewritten, nullptr)
            && writeFile(manifest, rewritten))
            context.verbose("copy " + text(manifest) + " to " + text(manifest));
        else
            context.verbose("unable to copy file from " + text(manifest) + " to " + text(manifest));

        const fs::path brpak = output / (output.filename().string() + BRPAK_EXTENSION);
        std::error_code error;
        // a pack that already held one keeps it, staged as it was
        if (fs::exists(brpak, error)) {
            context.verbose("Error packaging brpak for " + text(output) + " because " + text(brpak)
                            + " already exists");
            return;
        }

        std::vector<fs::path> files;
        collectFiles(output, files);
        std::vector<ZipEntry> entries;
        for (const fs::path &file: files) {
            ZipEntry entry;
            entry.mName = fs::relative(file, output, error).generic_string();
            if (error || !readFile(file, entry.mData)) {
                context.verbose("failed to write brpak: " + text(file));
                context.mFailed = true;
                return;
            }
            entries.push_back(std::move(entry));
        }

        uint16_t time = 0;
        uint16_t date = 0;
        dosTimestamp(time, date);
        std::string archive;
        if (!BrPak::write(entries, time, date, archive)) {
            context.verbose("failed to write brpak: " + text(brpak));
            context.mFailed = true;
            return;
        }

        std::vector<fs::path> staged;
        for (fs::directory_iterator it(output, error), end; !error && it != end; it.increment(error))
            staged.push_back(it->path());
        for (const fs::path &path: staged)
            fs::remove_all(path, error);
        if (!writeFile(brpak, archive)) {
            context.verbose("failed to write brpak: " + text(brpak));
            context.mFailed = true;
            return;
        }
        context.verbose("wrote brpak " + text(brpak) + " with " + std::to_string(entries.size()) + " entries");
    }

    bool isValidArchiveName(const std::string &name) {
        return !name.empty() && name != "." && name != ".." && name.find('/') == std::string::npos
               && name.find('\\') == std::string::npos && name.find(':') == std::string::npos;
    }

    void copyTree(Context &context, const fs::path &source, const fs::path &output, bool root) {
        std::vector<fs::path> files;
        std::vector<fs::path> folders;
        listDirectory(source, files, folders);
        for (const fs::path &file: files) {
            const std::string name = file.filename().string();
            copyFile(context, file, output / name, root && name == MANIFEST);
        }
        for (const fs::path &folder: folders) {
            const std::string name = folder.filename().string();
            if (root && name == ARCHIVE_FOLDER)
                continue;
            copyTree(context, folder, output / name, false);
        }
    }

    void extractArchives(Context &context, const fs::path &archives, const fs::path &output) {
        std::vector<fs::path> files;
        collectFiles(archives, files);
        for (const fs::path &file: files) {
            std::error_code error;
            const std::string relative = fs::relative(file, archives, error).generic_string();
            const size_t extension = relative.size() - std::string(ARCHIVE_EXTENSION).size();
            if (error || relative.size() <= std::string(ARCHIVE_EXTENSION).size()
                || relative.compare(extension, std::string::npos, ARCHIVE_EXTENSION) != 0)
                continue;
            const std::string folder = relative.substr(0, extension);
            std::string data;
            std::vector<BrArchive::Entry> entries;
            if (!readFile(file, data) || !BrArchive::read(data, entries)) {
                context.verbose("unable to read archive " + text(file));
                context.mFailed = true;
                continue;
            }
            for (const BrArchive::Entry &entry: entries) {
                // an empty entry is a file the folder already holds on the disk
                if (entry.mData.empty())
                    continue;
                if (!isValidArchiveName(entry.mName) || !writeFile(output / folder / entry.mName, entry.mData)) {
                    context.verbose("unable to extract " + folder + "/" + entry.mName);
                    context.mFailed = true;
                    continue;
                }
                context.verbose("extracted " + folder + "/" + entry.mName);
            }
        }
    }

    void unpackPack(Context &context, const fs::path &source, const fs::path &output) {
        std::vector<fs::path> files;
        std::vector<fs::path> folders;
        listDirectory(source, files, folders);
        // with several, the last one listed is used
        const fs::path *brpak = nullptr;
        for (const fs::path &file: files) {
            if (file.extension() == BRPAK_EXTENSION)
                brpak = &file;
        }

        if (brpak != nullptr) {
            std::string data;
            std::vector<ZipEntry> entries;
            if (!readFile(*brpak, data) || !ZipArchive::listEntries(data, entries)) {
                context.verbose("failed to open brpak for reading: " + text(*brpak));
                context.mFailed = true;
                return;
            }
            for (const ZipEntry &entry: entries) {
                if (!BrPak::isValidEntryName(entry.mName) || !writeFile(output / fs::path(entry.mName), entry.mData)) {
                    context.verbose("unable to extract " + entry.mName);
                    context.mFailed = true;
                    continue;
                }
                context.verbose("extracted " + entry.mName);
            }
            return;
        }

        copyTree(context, source, output, true);
        const fs::path archives = source / ARCHIVE_FOLDER;
        std::error_code error;
        if (!fs::is_directory(archives, error)) {
            context.verbose("no " + std::string(ARCHIVE_FOLDER) + " directory found, nothing to unpack");
            return;
        }
        extractArchives(context, archives, output);
    }

    // in place, a baked pack is left alone
    bool isAlreadyOptimized(const fs::path &pack) {
        std::string data;
        PackJson::Value document;
        if (!readFile(pack / MANIFEST, data) || data.size() > MAX_JSON_SIZE || !PackJson::parse(data, document)
            || document.mType != PackJson::Value::Type::Object)
            return false;
        PackJson::Value *header = document.find("header");
        if (header == nullptr || header->mType != PackJson::Value::Type::Object)
            return false;
        const PackJson::Value *version = header->find("pack_optimization_version");
        return version != nullptr && version->mType == PackJson::Value::Type::String;
    }

    void processPack(Context &context, const fs::path &source, const fs::path &output) {
        if (context.mMode == Mode::Unpack)
            unpackPack(context, source, output);
        else
            bakePack(context, source, output);
        if (context.mMode == Mode::Brpak)
            packBrpak(context, output);
    }

    std::vector<fs::path> componentsOf(const fs::path &path) {
        std::vector<fs::path> components;
        for (const fs::path &component: path) {
            if (!component.empty())
                components.push_back(component);
        }
        return components;
    }

    bool contains(const fs::path &outer, const fs::path &inner) {
        const std::vector<fs::path> a = componentsOf(outer);
        const std::vector<fs::path> b = componentsOf(inner);
        return a.size() <= b.size() && std::equal(a.begin(), a.end(), b.begin());
    }

    // one is the other or lies inside it, once both are resolved: writing the output would delete the input
    bool overlaps(const fs::path &source, const fs::path &target) {
        std::error_code error;
        const fs::path a = fs::weakly_canonical(source, error).lexically_normal();
        const fs::path b = fs::weakly_canonical(target, error).lexically_normal();
        return contains(a, b) || contains(b, a);
    }

    // the original is moved aside first and put back if the new pack can't take its place
    bool replacePack(const fs::path &target, const fs::path &staging, const fs::path &backup) {
        std::error_code error;
        fs::remove_all(backup, error);
        fs::rename(target, backup, error);
        if (error)
            return false;
        fs::rename(staging, target, error);
        if (error) {
            std::error_code restore;
            fs::rename(backup, target, restore);
            return false;
        }
        fs::remove_all(backup, error);
        return true;
    }

    std::string statsJson(const Stats &stats, long long milliseconds) {
        std::string uuid;
        PackJson::Value value;
        value.mType = PackJson::Value::Type::String;
        value.mString = stats.mPackUuid;
        PackJson::write(value, uuid);
        return "{\n\t\"Pack UUID\": " + uuid + ",\n\t\"Files Processed\": " + std::to_string(stats.mFilesProcessed)
               + ",\n\t\"Files Binary Transformed\": 0,\n\t\"Files JSON Minified\": "
               + std::to_string(stats.mFilesJsonMinified) + ",\n\t\"Files Already Binary\": 0,\n\t\"Files JSON Parse Error\": "
               + std::to_string(stats.mFilesJsonParseError) + ",\n\t\"Files Non-JSON\": "
               + std::to_string(stats.mFilesNonJson) + ",\n\t\"Archives Written\": "
               + std::to_string(stats.mArchivesWritten) + ",\n\t\"Subpacks Processed\": "
               + std::to_string(stats.mSubpacksProcessed) + ",\n\t\"Duration (ms)\": " + std::to_string(milliseconds)
               + "\n}";
    }

    bool readFlag(const PackJson::Value &config, const char *name, bool &out) {
        const PackJson::Value *value = config.find(name);
        if (value == nullptr)
            return true;
        if (value->mType != PackJson::Value::Type::True && value->mType != PackJson::Value::Type::False)
            return false;
        out = value->mType == PackJson::Value::Type::True;
        return true;
    }
}

bool PackOptimizer::loadConfig(const std::string &path, Config &config, std::string &error) {
    std::string data;
    PackJson::Value document;
    if (!readFile(path, data)) {
        error = "unable to read " + path;
        return false;
    }
    if (!PackJson::parse(data, document) || document.mType != PackJson::Value::Type::Object) {
        error = path + " is not a JSON object";
        return false;
    }
    const PackJson::Value *input = document.find("input_directory");
    const PackJson::Value *output = document.find("output_directory");
    if (input == nullptr || input->mType != PackJson::Value::Type::String || output == nullptr
        || output->mType != PackJson::Value::Type::String) {
        error = path + " needs input_directory and output_directory";
        return false;
    }
    config.mInputDirectory = input->mString;
    config.mOutputDirectory = output->mString;
    if (!readFlag(document, "verbose_logging", config.mVerbose) || !readFlag(document, "unpack", config.mUnpack)
        || !readFlag(document, "brpak", config.mBrpak)) {
        error = path + ": verbose_logging, unpack and brpak are booleans";
        return false;
    }
    return true;
}

int PackOptimizer::run(const std::string &configPath) {
    Config config;
    std::string error;
    if (!loadConfig(configPath, config, error)) {
        LOG_ERROR(LogAreaID::Resource, "PackOptimizer: %s", error.c_str());
        return EXIT_FAILED;
    }
    return run(config);
}

int PackOptimizer::run(const Config &config) {
    const fs::path input(config.mInputDirectory);
    const fs::path output(config.mOutputDirectory);
    std::error_code error;
    if (!fs::is_directory(input, error)) {
        LOG_ERROR(LogAreaID::Resource, "Input directory '%s' does not exist", config.mInputDirectory.c_str());
        return EXIT_FAILED;
    }
    fs::create_directories(output, error);
    if (!fs::is_directory(output, error)) {
        LOG_ERROR(LogAreaID::Resource, "Failed to create output directory '%s'", config.mOutputDirectory.c_str());
        return EXIT_FAILED;
    }

    const Mode mode = config.mUnpack ? Mode::Unpack : config.mBrpak ? Mode::Brpak : Mode::Bake;
    const char *action = mode == Mode::Unpack ? "unpack" : "optimize";
    const bool inPlace = fs::equivalent(input, output, error);

    std::vector<fs::path> files;
    std::vector<fs::path> packs;
    listDirectory(input, files, packs);
    LOG_INFO(LogAreaID::Resource, "PackOptimizer: Found %d packs in '%s'", (int) packs.size(),
             config.mInputDirectory.c_str());

    bool failed = false;
    for (const fs::path &pack: packs) {
        const std::string name = pack.filename().string();
        // left behind by an in-place run that stopped halfway
        if (inPlace && name == STAGING_FOLDER)
            continue;
        const fs::path target = output / name;
        LOG_INFO(LogAreaID::Resource, "PackOptimizer: %s pack '%s'", action, name.c_str());
        if (inPlace && mode == Mode::Bake && isAlreadyOptimized(pack)) {
            if (config.mVerbose)
                std::cout << "skip already optimized pack " << text(target) << '\n';
            continue;
        }

        const auto start = std::chrono::steady_clock::now();
        Context context{config, mode, {}};
        if (inPlace) {
            // staged under its own name, swapped in only once the whole pack went through
            const fs::path stagingRoot = output / STAGING_FOLDER;
            const fs::path staging = stagingRoot / name;
            fs::remove_all(staging, error);
            processPack(context, pack, staging);
            if (!context.mFailed && !replacePack(target, staging, stagingRoot / (name + ".original")))
                context.mFailed = true;
            if (context.mFailed)
                LOG_ERROR(LogAreaID::Resource, "PackOptimizer: pack '%s' left as it was", name.c_str());
            fs::remove_all(stagingRoot, error);
        } else if (overlaps(pack, target)) {
            LOG_ERROR(LogAreaID::Resource, "PackOptimizer: pack '%s' would be written over itself", name.c_str());
            context.mFailed = true;
        } else {
            fs::remove_all(target, error);
            processPack(context, pack, target);
        }
        const auto milliseconds =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        LOG_INFO(LogAreaID::Resource, "PackOptimizer: Completed. Stats:\n%s",
                 statsJson(context.mStats, (long long) milliseconds).c_str());
        failed = failed || context.mFailed;
    }
    std::cout.flush();

    if (failed) {
        LOG_ERROR(LogAreaID::Resource, "Failed to %s one or more packs", action);
        return EXIT_FAILED;
    }
    return 0;
}
