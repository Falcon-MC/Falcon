#include "Server/SetupWizard.h"

#include "Server/OpList.h"
#include "Server/PropertiesSettings.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {
    const size_t FRAME_WIDTH = 64;

    std::string toLower(const std::string &value) {
        std::string lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char character) { return (char) std::tolower(character); });
        return lowered;
    }

    std::string trim(const std::string &value) {
        size_t start = 0;
        size_t end = value.size();

        while (start < end && std::isspace((unsigned char) value[start]) != 0)
            ++start;

        while (end > start && std::isspace((unsigned char) value[end - 1]) != 0)
            --end;

        return value.substr(start, end - start);
    }

    std::string join(const std::vector<std::string> &values, const std::string &separator) {
        std::string joined;

        for (const std::string &value: values) {
            if (!joined.empty())
                joined += separator;
            joined += value;
        }

        return joined;
    }
}

SetupWizard::SetupWizard(const std::string &propertiesPath, const std::string &opsPath)
        : mPropertiesPath(propertiesPath), mOpsPath(opsPath) {}

bool SetupWizard::isNeeded(const std::string &propertiesPath) {
    std::ifstream file(propertiesPath);
    return !file.is_open();
}

bool SetupWizard::isInteractive() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

void SetupWizard::writeLine(const std::string &line) const {
    std::cout << line << std::endl;
}

void SetupWizard::message(const std::string &text) const {
    writeLine("[*] " + text);
}

void SetupWizard::warning(const std::string &text) const {
    writeLine("[!] " + text);
}

void SetupWizard::frame(const std::string &title) const {
    const std::string border(FRAME_WIDTH, '=');

    writeLine("");
    writeLine(border);
    writeLine("  " + title);
    writeLine(border);
}

std::string SetupWizard::readLine() const {
    std::string line;
    if (!std::getline(std::cin, line))
        return std::string();

    return trim(line);
}

std::string SetupWizard::ask(const std::string &prompt, const std::string &defaultValue) const {
    std::cout << "[?] " << prompt;

    if (!defaultValue.empty())
        std::cout << " (" << defaultValue << ")";

    std::cout << ": " << std::flush;

    const std::string input = readLine();
    return input.empty() ? defaultValue : input;
}

bool SetupWizard::askConfirmation(const std::string &prompt, bool defaultValue) const {
    while (true) {
        std::cout << "[?] " << prompt << (defaultValue ? " (Y/n): " : " (y/N): ") << std::flush;

        const std::string input = toLower(readLine());
        if (input.empty())
            return defaultValue;

        if (input == "y" || input == "yes")
            return true;

        if (input == "n" || input == "no")
            return false;

        warning("Please answer with y or n.");
    }
}

int SetupWizard::askInteger(const std::string &prompt, int defaultValue, int minimum, int maximum) const {
    while (true) {
        const std::string input = ask(prompt, std::to_string(defaultValue));

        char *end = nullptr;
        const long parsed = std::strtol(input.c_str(), &end, 10);

        if (end != input.c_str() && *end == '\0' && parsed >= minimum && parsed <= maximum)
            return (int) parsed;

        warning("Please enter a number between " + std::to_string(minimum) + " and " + std::to_string(maximum) + ".");
    }
}

std::string SetupWizard::askChoice(const std::string &prompt, const std::vector<std::string> &choices,
                                   const std::string &defaultValue) const {
    while (true) {
        const std::string input = toLower(ask(prompt + " [" + join(choices, "/") + "]", defaultValue));

        for (const std::string &choice: choices) {
            if (input == choice)
                return choice;
        }

        warning("Please choose one of: " + join(choices, ", ") + ".");
    }
}

void SetupWizard::configureServer() {
    frame("Server");

    mValues["server-name"] = ask("Server name", mValues["server-name"]);
    mValues["gamemode"] = askChoice("Default gamemode", {"survival", "creative", "adventure"},
                                    mValues["gamemode"]);
    mValues["difficulty"] = askChoice("Difficulty", {"peaceful", "easy", "normal", "hard"},
                                      mValues["difficulty"]);
    mValues["max-players"] = std::to_string(askInteger("Maximum players", atoi(mValues["max-players"].c_str()),
                                                       1, 1000));
}

void SetupWizard::configureNetwork() {
    frame("Network");

    message("The server ports must be reachable for players outside your network to join.");

    mValues["server-port"] = std::to_string(askInteger("IPv4 port", atoi(mValues["server-port"].c_str()),
                                                       1, 65535));
    mValues["server-portv6"] = std::to_string(askInteger("IPv6 port", atoi(mValues["server-portv6"].c_str()),
                                                         1, 65535));

    const bool onlineMode = askConfirmation("Require a Xbox Live account to join", true);
    mValues["online-mode"] = onlineMode ? "true" : "false";

    if (!onlineMode)
        warning("Anyone can join with any name while online-mode is disabled.");

    mValues["enable-lan-visibility"] = askConfirmation("Announce the server on the local network", true)
                                       ? "true" : "false";
}

void SetupWizard::configureWorld() {
    frame("World");

    mValues["level-name"] = ask("World name", mValues["level-name"]);
    mValues["level-seed"] = ask("World seed, leave empty for a random one", mValues["level-seed"]);
    mValues["view-distance"] = std::to_string(askInteger("View distance in chunks",
                                                         atoi(mValues["view-distance"].c_str()), 4, 96));
}

void SetupWizard::configurePlayers() {
    frame("Players");

    mOperator = ask("Name of the first operator, leave empty for none", "");

    if (mOperator.empty())
        warning("No operator was set, nobody will be able to run administrative commands.");

    const bool allowList = askConfirmation("Only let players from allowlist.json join", false);
    mValues["allow-list"] = allowList ? "true" : "false";

    if (allowList)
        warning("The allowlist is empty, add players with the allowlist command before they can join.");
}

bool SetupWizard::save() {
    std::ofstream file(mPropertiesPath);
    if (!file.is_open()) {
        warning("Could not write " + mPropertiesPath);
        return false;
    }

    for (const PropertyDefinition &definition: PropertiesSettings::getDefinitions()) {
        const auto it = mValues.find(definition.mKey);
        file << definition.mKey << "=" << (it == mValues.end() ? definition.mDefault : it->second) << "\n";
    }

    file.close();

    if (!mOperator.empty()) {
        OpList ops(mOpsPath);
        ops.addOp(mOperator);
    }

    return true;
}

void SetupWizard::printSummary() const {
    frame("Summary");

    message("Server name: " + mValues.at("server-name"));
    message("Gamemode: " + mValues.at("gamemode") + ", difficulty: " + mValues.at("difficulty"));
    message("Maximum players: " + mValues.at("max-players"));
    message("Ports: " + mValues.at("server-port") + " (IPv4), " + mValues.at("server-portv6") + " (IPv6)");
    message("World: " + mValues.at("level-name"));
    message("Operator: " + (mOperator.empty() ? std::string("none") : mOperator));
    writeLine("");
    message("Settings were written to " + mPropertiesPath + " and can be edited at any time.");
    writeLine("");
}

bool SetupWizard::run() {
    for (const PropertyDefinition &definition: PropertiesSettings::getDefinitions())
        mValues[definition.mKey] = definition.mDefault;

    frame("Falcon setup");

    message("This wizard runs once, because no " + mPropertiesPath + " was found.");
    message("Press enter to keep the value shown in parentheses.");

    if (!askConfirmation("Configure the server now", true)) {
        message("Keeping the default settings.");
        return save();
    }

    configureServer();
    configureNetwork();
    configureWorld();
    configurePlayers();

    if (!save())
        return false;

    printSummary();
    return true;
}
