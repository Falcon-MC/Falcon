#include "Core/Archive/ZipArchive.h"
#include "Resources/BrArchive.h"
#include "Resources/BrPak.h"
#include "Resources/PackJson.h"
#include "Resources/PackOptimizer.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::string minified(const std::string &text) {
        std::string out;
        require(PackJson::minify(text, out), "the game minifies " + text);
        return out;
    }

    void writeFile(const fs::path &path, const std::string &data) {
        fs::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file << data;
    }

    std::string readFile(const fs::path &path) {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    // inputs and outputs of the dedicated server 1.26.65 optimizer
    void testNumbers() {
        require(minified("[0.30000000000000004, 1.7976931348623157e308, 5e-324, 2.2250738585072014e-308, "
                         "123456789012345678, 1e21, 1e20, 1.5e-7, 0.000001, 1e-7, -1.0, 3.0e2, 1E2, 0.1e1, "
                         "9007199254740993, -9223372036854775808, -9223372036854775809, 18446744073709551615, "
                         "18446744073709551616, 2147483647, 2147483648, -2147483648, -2147483649, "
                         "1.00000000000000011, 0.1234567890123456789]")
                        == "[0.30000000000000007,1.7976931348623157e308,5e-324,2.2250738585072014e-308,"
                           "123456789012345678,1e21,100000000000000000000.0,1.5e-7,0.000001,1e-7,-1.0,300.0,100.0,"
                           "1.0,9007199254740993,-9223372036854775808,-9223372036854776000.0,18446744073709551615,"
                           "18446744073709553000.0,2147483647,2147483648,-2147483648,-2147483649,1.0,"
                           "0.12345678901234568]",
                "number formatting");
        require(minified("[123456789012345678901234567890, -123456789012345678901234567890, 1.5E+3, "
                         "0.00000000000000000000000000001234, 3.14159265358979323846264338327950288]")
                        == "[1.2345678901234566e29,-1.2345678901234566e29,1500.0,1.234e-29,3.141592653589793]",
                "long numbers");
        require(minified("[1e-99999999999]") == "[0.0]", "underflow");

        const std::pair<const char *, const char *> SAMPLES[] = {
                {"2.3678608119e+180", "2.3678608119000003e180"},
                {"9e-275", "9.000000000000001e-275"},
                {"7e161", "7.000000000000001e161"},
                {"0.472803852808414852538885", "0.47280385280841488"},
                {"0.90751163726516761222029729975", "0.9075116372651677"},
                {"-445628.91941747384", "-445628.91941747386"},
                {"31401419314170.86492083124", "31401419314170.869"},
                {"1e-34", "1.0000000000000002e-34"},
                {"110423468.35606883106", "110423468.35606884"},
                {"3.679946790e-54", "3.6799467899999999e-54"},
                {"12390424861114893643900847.537838306", "1.2390424861114895e25"},
                {"611253287046557210141561.365461073587355706", "6.112532870465573e23"},
        };
        for (const auto &[input, expected]: SAMPLES)
            require(minified(input) == expected, std::string("number ") + input);

        for (const char *refused: {"[1e400]", "[1.7976931348623158e308]", "[NaN]", "[Infinity]", "[01]", "[.5]",
                                   "[-]", "[1.]", "[1e]", "[1e+]"}) {
            std::string out;
            require(!PackJson::minify(refused, out), std::string("refused ") + refused);
        }
    }

    void testSyntax() {
        require(minified("\xEF\xBB\xBF{ \"n\" : 1.0, \"m\": 1e5, \"big\": 12345678901234567890, \"neg\": -0, \"f\": 0.1, "
                         "\"u\": \"\\u00e9\\n\", \"dup\": 1, \"dup\": 2, \"t\": true, \"z\": null, \"arr\": [ ] , "
                         "\"obj\": { } }")
                        == "{\"n\":1.0,\"m\":100000.0,\"big\":12345678901234567890,\"neg\":0,\"f\":0.1,"
                           "\"u\":\"\xC3\xA9\\n\",\"dup\":1,\"dup\":2,\"t\":true,\"z\":null,\"arr\":[],\"obj\":{}}",
                "object, duplicates and byte order mark");
        require(minified("[\"\\/\", \"\\b\\f\\n\\r\\t\", \"\\u0001\\u001f\\u007f\", \"\\ud83d\\ude00\", "
                         "\"\\u00e9\", \"a\\\"b\\\\c\", \"\\u2028\\u2029\", \"\\u0000x\"]")
                        == std::string("[\"/\",\"\\b\\f\\n\\r\\t\",\"\\u0001\\u001F\x7F\",\"\xF0\x9F\x98\x80\","
                                       "\"\xC3\xA9\",\"a\\\"b\\\\c\",\"\xE2\x80\xA8\xE2\x80\xA9\",\"\\u0000x\"]"),
                "strings");
        require(minified("[\"\\udc00\"]") == "[\"\xED\xB0\x80\"]", "lone low surrogate");
        require(minified("// line comment\n{ \"a\": 1 /* inner */ }") == "{\"a\":1}", "comments");
        require(minified("[1] /* a **/") == "[1]", "closing comment");
        require(minified("{\"a\":1} // c") == "{\"a\":1}", "trailing line comment");
        require(minified(std::string("{\"a\":1}\0garbage", 15)) == "{\"a\":1}", "NUL ends the document");
        require(minified("\xEF[1]") == "[1]" && minified("\xBB\xBF[1]") == "[1]", "partial byte order mark");
        require(minified("42") == "42" && minified("\"str\"") == "\"str\"", "scalar root");
        require(minified("[\"\xFF\xFE bad\"]") == "[\"\xFF\xFE bad\"]", "bytes are not validated");

        for (const std::string &refused:
             {std::string("{ \"a\": [1, 2,], }"), std::string(""), std::string("   "), std::string("{\"a\":1} x"),
              std::string("{\"a\":1}{\"b\":2}"), std::string("{'a':1}"), std::string("\f[1]"),
              std::string("[\"tab\there\"]"), std::string("[\"\\a\"]"), std::string("[\"\\u12G4\"]"),
              std::string("{\"a\":1,}"), std::string("[1 2]"), std::string("{\"a\" 1}"), std::string("/* open"),
              std::string("[1] /"), std::string("nul"), std::string("[True]"),
              std::string("\xEF\xBB\xBF\xEF\xBB\xBF[1]"), std::string("{\"a\":\"x\0y\"}", 11)}) {
            std::string out;
            require(!PackJson::minify(refused, out), "refused " + refused);
        }
    }

    void testBrArchive() {
        std::string archive;
        require(BrArchive::write({{"x.json", "{}"}, {"UPPER.JSON", ""}, {"_a.json", "[1]"}}, archive),
                "archive written");
        require(archive.size() == 16 + 3 * 256 + 5, "archive size");
        require(archive.compare(0, 8, "\x7D\x27\x25\xB1\xA0\x52\x70\x26") == 0, "magic");
        require(archive[8] == 3 && archive[12] == 1, "count and version");
        // byte order: upper case, then underscore, then lower case
        require(archive.compare(17, 10, "UPPER.JSON") == 0 && archive.compare(16 + 256 + 1, 7, "_a.json") == 0,
                "sorted names");
        const size_t underscore = 16 + 256;
        require((unsigned char) archive[underscore + 248] == 0 && (unsigned char) archive[underscore + 252] == 3,
                "first data at offset 0");
        const size_t json = 16 + 512;
        require((unsigned char) archive[json + 248] == 3 && (unsigned char) archive[json + 252] == 2,
                "offsets add up");

        std::vector<BrArchive::Entry> entries;
        require(BrArchive::read(archive, entries) && entries.size() == 3 && entries[2].mData == "{}", "read back");
        require(!BrArchive::write({{std::string(247, 'a'), "x"}}, archive), "name too long");
        require(BrArchive::write({{std::string(246, 'a'), "x"}}, archive), "longest name");
    }

    void testBrPak() {
        std::string zip;
        require(BrPak::write({{"manifest.json", "{}"}, {"ui/a.json", "[]"}}, 0x4E28, 0x5D72, zip), "brpak written");
        require(zip.compare(zip.size() - 7, 7, BrPak::COMMENT) == 0, "comment");
        require(zip.compare(0, 10, std::string("PK\x03\x04\x0A\x00\x00\x08\x00\x00", 10)) == 0, "local header");
        std::vector<ZipEntry> entries;
        require(ZipArchive::listEntries(zip, entries) && entries.size() == 2 && entries[1].mName == "ui/a.json",
                "read back");
        const std::string_view greeting("\xcb\x48\xcd\xc9\xc9\x07\x00", 7);
        std::string decoded;
        require(ZipArchive::readEntry(greeting, 8, 5, 5, decoded) && decoded == "hello", "deflated entry");
        require(!ZipArchive::readEntry(greeting, 8, 6, 6, decoded), "decoded size must match the entry");
        require(!ZipArchive::readEntry(greeting, 8, 5, 4, decoded), "entry exceeds its budget");
        std::vector<ZipEntry> limited;
        require(!ZipArchive::listEntries(zip, limited, 3) && limited.empty(), "archive budget is atomic");
        require(ZipArchive::listEntries(zip, limited, 4) && limited.size() == 2, "exact archive budget");
        require(!BrPak::write({{"a", "1"}, {"a", "2"}}, 0, 0, zip), "duplicate names refused");
        require(!BrPak::isValidEntryName("../x") && !BrPak::isValidEntryName("/x") && !BrPak::isValidEntryName("a\\b"),
                "unsafe names refused");
    }

    void testOptimizer() {
        const fs::path root = fs::temp_directory_path() / "falcon-pack-optimizer-tests";
        fs::remove_all(root);
        const fs::path pack = root / "in" / "rp";
        writeFile(pack / "manifest.json", "// c\n{ \"header\": { \"name\": \"R\", \"uuid\": \"u\" } }");
        writeFile(pack / "blocks.json", "{ \"a\" : 1 }");
        writeFile(pack / "ui" / "hud.json", "{ \"x\" : 1 }");
        writeFile(pack / "ui" / "broken.json", "{ \"a\": [1, 2,], }");
        writeFile(pack / "texts" / "en_US.lang", "a=b");
        writeFile(pack / "texts" / "languages.json", "[ \"en_US\" ]");
        writeFile(pack / "textures" / "blocks" / "stone.png", "png");
        writeFile(pack / "subpacks" / "low" / "textures" / "x.json", "{ \"a\" : 1 }");

        PackOptimizer::Config config;
        config.mInputDirectory = (root / "in").string();
        config.mOutputDirectory = (root / "baked").string();
        require(PackOptimizer::run(config) == 0, "bake");
        const fs::path baked = root / "baked" / "rp";
        require(readFile(baked / "manifest.json")
                        == "{\"header\":{\"name\":\"R\",\"uuid\":\"u\",\"pack_optimization_version\":\"0.1.0\"}}",
                "manifest tagged");
        require(readFile(baked / "blocks.json") == "{ \"a\" : 1 }", "root files copied as they are");
        require(!fs::exists(baked / "ui" / "hud.json") && readFile(baked / "ui" / "broken.json") == "{ \"a\": [1, 2,], }",
                "only a broken JSON stays beside the archive");
        require(readFile(baked / "texts" / "languages.json") == "[\"en_US\"]", "a loose copy kept minified");
        require(fs::exists(baked / "__brarchive" / "textures.brarchive")
                        && fs::exists(baked / "__brarchive" / "textures" / "blocks.brarchive"),
                "an empty archive above a baked folder");
        require(fs::exists(baked / "subpacks" / "low" / "__brarchive" / "textures.brarchive"), "subpack baked");
        std::vector<BrArchive::Entry> entries;
        require(BrArchive::read(readFile(baked / "__brarchive" / "ui.brarchive"), entries) && entries.size() == 2
                        && entries[0].mName == "broken.json" && entries[0].mData.empty()
                        && entries[1].mData == "{\"x\":1}",
                "ui archive");

        config.mOutputDirectory = (root / "packed").string();
        config.mBrpak = true;
        require(PackOptimizer::run(config) == 0, "brpak");
        const fs::path brpak = root / "packed" / "rp" / "rp.brpak";
        std::vector<ZipEntry> files;
        require(ZipArchive::listEntries(readFile(brpak), files) && files.size() == 8, "every file packed");
        require(files[0].mName == "blocks.json" && files.back().mName == "ui/hud.json", "listing order");

        config.mInputDirectory = (root / "packed").string();
        config.mOutputDirectory = (root / "unpacked").string();
        config.mBrpak = false;
        config.mUnpack = true;
        require(PackOptimizer::run(config) == 0, "unpack");
        require(readFile(root / "unpacked" / "rp" / "ui" / "hud.json") == "{\"x\":1}", "unpacked file");
        require(readFile(root / "unpacked" / "rp" / "manifest.json").find("\"1.0.0\"") != std::string::npos,
                "packed manifest kept");

        config.mInputDirectory = (root / "missing").string();
        require(PackOptimizer::run(config) == 7, "missing input");

        // the output of pack "in" would be the folder holding it
        const fs::path nested = root / "overlap" / "in" / "in";
        writeFile(nested / "manifest.json", "{\"header\":{}}");
        config.mInputDirectory = (root / "overlap" / "in").string();
        config.mOutputDirectory = (root / "overlap").string();
        config.mUnpack = false;
        require(PackOptimizer::run(config) == 7 && fs::exists(nested / "manifest.json"), "overlapping paths refused");

        // in place, a pack that fails keeps its files: here a brpak entry pointing out of the pack
        const fs::path failing = root / "inplace" / "p";
        ZipWriter unsafe;
        unsafe.addFile("ok.txt", "ok");
        unsafe.addFile("../escape.txt", "x");
        writeFile(failing / "p.brpak", unsafe.finish());
        config.mInputDirectory = (root / "inplace").string();
        config.mOutputDirectory = config.mInputDirectory;
        config.mUnpack = true;
        require(PackOptimizer::run(config) == 7 && fs::exists(failing / "p.brpak") && !fs::exists(failing / "ok.txt")
                        && !fs::exists(root / "inplace" / "escape.txt"),
                "failed pack left as it was");
        fs::remove_all(root);
    }
}

int main() {
    try {
        testNumbers();
        testSyntax();
        testBrArchive();
        testBrPak();
        testOptimizer();
    } catch (const std::exception &error) {
        std::cerr << "pack optimizer check failed: " << error.what() << std::endl;
        return 1;
    }
    std::cout << "pack optimizer checks passed" << std::endl;
    return 0;
}
