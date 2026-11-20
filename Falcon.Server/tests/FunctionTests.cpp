#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Server/Commands/CommandMap.h"
#include "Server/Commands/ServerCommandOrigin.h"
#include "Server/Functions/FunctionManager.h"
#include "Server/ServerPaths.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void writeFile(const std::filesystem::path &path, const std::string &text) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << text;
    }

    // records what ran and what the origin saw
    class NoteCommand : public Command {
    public:
        explicit NoteCommand(std::vector<std::string> &notes) : Command("note", "Notes", "/note"), mNotes(notes) {}

        bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override {
            mNotes.push_back(arguments.empty() ? std::string() : arguments[0]);
            sender.sendMessage("noted");
            return true;
        }

    private:
        std::vector<std::string> &mNotes;
    };

    class RecordingOrigin : public ServerCommandOrigin {
    public:
        explicit RecordingOrigin(ServerNetworkHandler *handler) : ServerCommandOrigin(handler) {}

        void sendMessage(const std::string &message) override {
            mLines.push_back(message);
        }

        void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) override {
            std::string line = key;
            for (const std::string &parameter: parameters)
                line += "|" + parameter;
            mLines.push_back(line);
        }

        std::vector<std::string> mLines;
    };

    void run(const std::filesystem::path &directory) {
        const std::filesystem::path pack = directory / "pack";
        writeFile(pack / "functions" / "main.mcfunction",
                  "# a comment\r\n\r\n  note one  \r\nfunction folder/inner\r\nnote three\r\n");
        writeFile(pack / "functions" / "folder" / "inner.mcfunction", "note two\n");
        writeFile(pack / "functions" / "broken.mcfunction", "note fine\n/note slash\n// comment\nnothing here\n");
        writeFile(pack / "functions" / "loop.mcfunction", "note loop\nfunction loop\n");
        writeFile(pack / "functions" / "tick.json", R"({"values": ["folder/inner", "missing"]})");
        BehaviorPack behavior;
        behavior.mName = "functions";
        behavior.mPath = pack.string();

        ServerNetworkHandler owner("Functions", "", 1);
        std::vector<std::string> notes;
        owner.getCommands().registerCommand(std::make_shared<NoteCommand>(notes));
        FunctionManager &functions = owner.getFunctions();
        functions.load({behavior}, owner.getCommands());

        require(functions.getFunctionCount() == 4 && functions.find("folder/inner") != nullptr,
                "the functions were not named by their path");
        const FunctionManager::Function *main = functions.find("main");
        require(main != nullptr && main->mCommands.size() == 3 && main->mCommands[0] == "note one",
                "comments and blank lines were kept, or the lines not trimmed");
        require(functions.find("broken")->mFailed, "a function naming no command loaded");
        require(functions.getTickFunctions().size() == 1 && functions.getTickFunctions()[0] == "folder/inner",
                "tick.json kept a function that does not exist");

        // the nested function runs in place; its own entry counts, as the game counts it
        RecordingOrigin console(&owner);
        require(owner.getCommands().dispatch(console, "function main"), "/function main failed");
        require(notes == std::vector<std::string>({"one", "two", "three"}), "the commands did not run in order");
        require(console.mLines == std::vector<std::string>({"commands.function.success|4"}),
                "the function did not report alone, or the count is not the game's");

        console.mLines.clear();
        notes.clear();
        require(!owner.getCommands().dispatch(console, "function broken")
                        && console.mLines.back() == "commands.generic.exception" && notes.empty(),
                "a function that failed to load ran");
        require(!owner.getCommands().dispatch(console, "function nope")
                        && console.mLines.back() == "commands.function.functionNameNotFound|nope",
                "an unknown function was not reported");
        require(!owner.getCommands().dispatch(console, "function bad!name")
                        && console.mLines.back() == "commands.function.invalidCharacters|bad!name|!",
                "a name with a bad character was not refused");

        // a function calling itself stops at the chain length
        require(owner.getLevel().getGameRules().setFromString("maxcommandchainlength", "10"), "no chain length rule");
        console.mLines.clear();
        require(owner.getCommands().dispatch(console, "function loop"), "/function loop failed");
        require(notes.size() == 5 && console.mLines.back() == "commands.function.success|10",
                "the chain length did not stop the loop: " + console.mLines.back());

        notes.clear();
        functions.tick(console, owner.getCommands(), 100);
        require(notes == std::vector<std::string>({"two"}), "the tick function did not run");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-functions-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
        std::cout << "function checks passed\n";
    } catch (const std::exception &error) {
        std::clog << "Function regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
