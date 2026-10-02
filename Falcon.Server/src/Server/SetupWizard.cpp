#include "Server/SetupWizard.h"

#include "BuildInfo.h"
#include "Core/Net/HttpsClient.h"
#include "Server/AllowList.h"
#include "Server/Localization.h"
#include "Server/OpList.h"
#include "Server/PropertiesSettings.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
    const int STEP_COUNT = 4;
    const size_t FRAME_WIDTH = 60;
    const int EXTERNAL_IP_TIMEOUT_SECONDS = 3;
    const char *EXTERNAL_IP_URL = "https://api.ipify.org";

    const char *RESET = "\033[0m";
    const char *BOLD = "\033[1m";
    const char *DIM = "\033[90m";
    const char *ACCENT = "\033[38;5;141m";
    const char *GREEN = "\033[32m";
    const char *YELLOW = "\033[33m";
    const char *CYAN = "\033[36m";

    const char *LOGO[] = {
            R"( _____     _                 )",
            R"(|  ___|_ _| | ___ ___  _ __  )",
            R"(| |_ / _` | |/ __/ _ \| '_ \ )",
            R"(|  _| (_| | | (_| (_) | | | |)",
            R"(|_|  \__,_|_|\___\___/|_| |_|)"
    };

    const char *LICENSE_TEXT[] = {
            "Falcon is free software: you can redistribute it and/or modify",
            "it under the terms of the GNU Lesser General Public License as published",
            "by the Free Software Foundation, either version 3 of the License, or",
            "(at your option) any later version.",
            "",
            "Falcon is distributed in the hope that it will be useful,",
            "but WITHOUT ANY WARRANTY; without even the implied warranty of",
            "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.",
            "",
            "See the full license at https://www.gnu.org/licenses/lgpl-3.0.html"
    };

    std::string toLower(const std::string &value) {
        std::string lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char character) {
                           return (char) std::tolower(character);
                       });
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

    size_t displayWidth(const std::string &text) {
        size_t width = 0;

        for (unsigned char character: text) {
            if ((character & 0xC0) != 0x80)
                ++width;
        }

        return width;
    }

    std::string padRight(const std::string &text, size_t width) {
        const size_t current = displayWidth(text);
        return current >= width ? text : text + std::string(width - current, ' ');
    }

    std::string center(const std::string &text, size_t width) {
        const size_t current = displayWidth(text);
        if (current >= width)
            return text;

        return std::string((width - current) / 2, ' ') + text;
    }

    std::string repeat(const std::string &unit, size_t count) {
        std::string result;
        result.reserve(unit.size() * count);

        for (size_t index = 0; index < count; ++index)
            result += unit;

        return result;
    }

    std::string findLocalAddress() {
#ifdef _WIN32
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
            return std::string();
        SOCKET descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (descriptor == INVALID_SOCKET) {
            WSACleanup();
            return std::string();
        }
#else
        int descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (descriptor < 0)
            return std::string();
#endif

        sockaddr_in remote{};
        remote.sin_family = AF_INET;
        remote.sin_port = htons(53);
        remote.sin_addr.s_addr = htonl(0x08080808);

        std::string address;
        if (connect(descriptor, (sockaddr *) &remote, sizeof(remote)) == 0) {
            sockaddr_in local{};
            socklen_t length = sizeof(local);

            if (getsockname(descriptor, (sockaddr *) &local, &length) == 0) {
                const uint32_t value = ntohl(local.sin_addr.s_addr);
                address = std::to_string((value >> 24) & 0xFF) + "." + std::to_string((value >> 16) & 0xFF) + "."
                          + std::to_string((value >> 8) & 0xFF) + "." + std::to_string(value & 0xFF);
            }
        }

#ifdef _WIN32
        closesocket(descriptor);
        WSACleanup();
#else
        close(descriptor);
#endif
        return address;
    }

    std::string findExternalAddress() {
        std::string body;
        if (!HttpsClient::get(EXTERNAL_IP_URL, body, EXTERNAL_IP_TIMEOUT_SECONDS))
            return std::string();

        const std::string address = trim(body);
        const bool valid = !address.empty() && address.size() <= 45
                           && std::all_of(address.begin(), address.end(), [](unsigned char character) {
                               return std::isxdigit(character) != 0 || character == '.' || character == ':';
                           });
        return valid ? address : std::string();
    }
}

SetupWizard::SetupWizard(const std::string &propertiesPath, const std::string &opsPath,
                         const std::string &allowListPath)
        : mPropertiesPath(propertiesPath), mOpsPath(opsPath), mAllowListPath(allowListPath),
          mLocale(Localization::DEFAULT_LOCALE) {}

bool SetupWizard::isNeeded(const std::string &propertiesPath) {
    std::ifstream file(propertiesPath);
    return !file.is_open();
}

bool SetupWizard::isInteractive() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0 && _isatty(_fileno(stdout)) != 0;
#else
    return isatty(STDIN_FILENO) != 0 && isatty(STDOUT_FILENO) != 0;
#endif
}

void SetupWizard::prepareConsole() {
#ifdef _WIN32
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;

    if (output != INVALID_HANDLE_VALUE && GetConsoleMode(output, &mode) != 0)
        mUseColor = SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;

    mUseUnicode = SetConsoleOutputCP(CP_UTF8) != 0;
    SetConsoleCP(CP_UTF8);
#else
    const char *term = std::getenv("TERM");
    mUseColor = term != nullptr && std::string(term) != "dumb";

    const char *locale = std::getenv("LANG");
    const std::string lang = locale == nullptr ? std::string() : toLower(locale);
    mUseUnicode = lang.find("utf-8") != std::string::npos || lang.find("utf8") != std::string::npos;
#endif

    if (std::getenv("NO_COLOR") != nullptr)
        mUseColor = false;
}

std::string SetupWizard::tr(const std::string &key, const std::vector<std::string> &parameters) const {
    return Localization::getInstance().translate(mLocale, "falcon.wizard." + key, parameters);
}

std::string SetupWizard::paint(const std::string &text, const char *color) const {
    if (!mUseColor)
        return text;

    return std::string(color) + text + RESET;
}

void SetupWizard::writeLine(const std::string &line) const {
    std::cout << line << "\n" << std::flush;
}

void SetupWizard::notice(const std::string &text) const {
    writeLine(paint("  " + std::string(mUseUnicode ? "•" : "*") + " ", CYAN) + text);
}

void SetupWizard::accept(const std::string &text) const {
    writeLine(paint("  " + std::string(mUseUnicode ? "✓" : "[OK]") + " ", GREEN) + text);
}

void SetupWizard::warning(const std::string &text) const {
    writeLine(paint("  " + std::string(mUseUnicode ? "⚠" : "[!]") + " " + text, YELLOW));
}

void SetupWizard::hint(const std::string &text) const {
    writeLine(paint("    " + text, DIM));
}

std::string SetupWizard::border() const {
    return repeat(mUseUnicode ? "═" : "=", FRAME_WIDTH);
}

std::string SetupWizard::separator() const {
    return repeat(mUseUnicode ? "─" : "-", FRAME_WIDTH);
}

void SetupWizard::banner() const {
    writeLine();

    for (const char *line: LOGO)
        writeLine(paint(center(line, FRAME_WIDTH), ACCENT));

    writeLine();
    writeLine(paint(center("Setup wizard  " + std::string(mUseUnicode ? "·" : "-") + "  v"
                           + FalconBuildInfo::kVersion, FRAME_WIDTH), BOLD));
    writeLine(paint(border(), ACCENT));
    writeLine();
}

void SetupWizard::section(const std::string &label, const std::string &title) const {
    writeLine();
    writeLine(paint(separator(), DIM));
    writeLine(paint("  " + label, ACCENT) + paint("  " + title, BOLD));
    writeLine(paint(separator(), DIM));
    writeLine();
}

void SetupWizard::step(const std::string &title, const std::string &description) {
    ++mStep;

    writeLine();
    writeLine(paint(separator(), DIM));
    writeLine(paint("  " + tr("step", {std::to_string(mStep), std::to_string(STEP_COUNT)}), ACCENT)
              + paint("  " + title, BOLD));
    hint(description);
    writeLine(paint(separator(), DIM));
    writeLine();
}

void SetupWizard::selectLanguage(const std::string &language) {
    const Localization &localization = Localization::getInstance();

    if (!language.empty() && localization.hasLocale(language)) {
        mLocale = language;
        accept(tr("language.selected", {localization.getLanguageName(mLocale)}));
        return;
    }

    section("LANGUAGE", "Choose a language");

    std::vector<Choice> choices;
    for (const std::string &locale: localization.getLocales())
        choices.push_back({toLower(locale), localization.getLanguageName(locale) + paint("  " + locale, DIM)});

    printChoices(choices, toLower(Localization::DEFAULT_LOCALE));

    while (true) {
        const std::string input = toLower(ask("Language", toLower(Localization::DEFAULT_LOCALE)));

        char *end = nullptr;
        const long number = std::strtol(input.c_str(), &end, 10);
        const bool isNumber = end != input.c_str() && *end == '\0' && number >= 1 && number <= (long) choices.size();
        const std::string wanted = isNumber ? choices[number - 1].mValue : input;

        for (const std::string &locale: localization.getLocales()) {
            if (toLower(locale) != wanted)
                continue;

            mLocale = locale;
            accept(tr("language.selected", {localization.getLanguageName(mLocale)}));
            return;
        }

        warning("Please enter a number or a language code from the list.");
    }
}

bool SetupWizard::acceptLicense(bool licenseAccepted) const {
    section(tr("license.label"), "GNU Lesser General Public License v3.0");

    if (licenseAccepted) {
        accept(tr("license.acceptedByArgument"));
        return true;
    }

    for (const char *line: LICENSE_TEXT)
        writeLine("    " + std::string(line));

    writeLine();

    if (askConfirmation(tr("license.question"), false)) {
        accept(tr("license.accepted"));
        return true;
    }

    warning(tr("license.refused"));
    return false;
}

std::string SetupWizard::readLine() const {
    std::string line;
    if (!std::getline(std::cin, line))
        return std::string();

    return trim(line);
}

std::string SetupWizard::ask(const std::string &prompt, const std::string &defaultValue) const {
    std::cout << paint("  " + std::string(mUseUnicode ? "›" : ">") + " ", ACCENT) << prompt;

    if (!defaultValue.empty())
        std::cout << paint(" [" + defaultValue + "]", DIM);

    std::cout << ": " << std::flush;

    const std::string input = readLine();
    return input.empty() ? defaultValue : input;
}

bool SetupWizard::askConfirmation(const std::string &prompt, bool defaultValue) const {
    while (true) {
        const std::string input = toLower(ask(prompt + (defaultValue ? " (Y/n)" : " (y/N)"), ""));

        if (input.empty())
            return defaultValue;

        if (input == "y" || input == "yes")
            return true;

        if (input == "n" || input == "no")
            return false;

        warning(tr("invalid.confirmation"));
    }
}

int SetupWizard::askInteger(const std::string &prompt, int defaultValue, int minimum, int maximum) const {
    while (true) {
        const std::string input = ask(prompt, std::to_string(defaultValue));

        char *end = nullptr;
        const long parsed = std::strtol(input.c_str(), &end, 10);

        if (end != input.c_str() && *end == '\0' && parsed >= minimum && parsed <= maximum)
            return (int) parsed;

        warning(tr("invalid.number", {std::to_string(minimum), std::to_string(maximum)}));
    }
}

void SetupWizard::printChoices(const std::vector<Choice> &choices, const std::string &defaultValue) const {
    const std::string defaultLabel = "(" + tr("default") + ")";

    for (size_t index = 0; index < choices.size(); ++index) {
        const bool isDefault = choices[index].mValue == defaultValue;
        writeLine(paint("    [" + std::to_string(index + 1) + "] ", ACCENT) + choices[index].mLabel
                  + (isDefault ? paint("  " + defaultLabel, DIM) : ""));
    }

    writeLine();
}

std::string SetupWizard::askChoice(const std::string &prompt, const std::vector<Choice> &choices,
                                   const std::string &defaultValue) const {
    printChoices(choices, defaultValue);

    while (true) {
        const std::string input = toLower(ask(prompt, defaultValue));

        char *end = nullptr;
        const long number = std::strtol(input.c_str(), &end, 10);
        if (end != input.c_str() && *end == '\0' && number >= 1 && number <= (long) choices.size())
            return choices[number - 1].mValue;

        for (const Choice &choice: choices) {
            if (input == choice.mValue)
                return choice.mValue;
        }

        warning(tr("invalid.choice", {std::to_string(choices.size())}));
    }
}

std::vector<std::string> SetupWizard::askNames(const std::string &prompt) const {
    const std::string input = ask(prompt, "");
    std::vector<std::string> names;
    size_t start = 0;

    while (start <= input.size()) {
        size_t comma = input.find(',', start);
        if (comma == std::string::npos)
            comma = input.size();

        const std::string name = trim(input.substr(start, comma - start));
        if (!name.empty() && std::find(names.begin(), names.end(), name) == names.end())
            names.push_back(name);

        start = comma + 1;
    }

    return names;
}

void SetupWizard::configureServer() {
    step(tr("server.title"), tr("server.description"));

    mValues["server-name"] = ask(tr("server.name"), mValues["server-name"]);
    accept(tr("server.name.set", {paint(mValues["server-name"], BOLD)}));
    writeLine();

    mValues["gamemode"] = askChoice(tr("server.gamemode"), {
            {"survival", tr("gamemode.survival")},
            {"creative", tr("gamemode.creative")},
            {"adventure", tr("gamemode.adventure")},
            {"spectator", tr("gamemode.spectator")}
    }, mValues["gamemode"]);
    accept(tr("server.gamemode.set", {paint(tr("gamemode." + mValues["gamemode"]), BOLD)}));
    writeLine();

    mValues["difficulty"] = askChoice(tr("server.difficulty"), {
            {"peaceful", tr("difficulty.peaceful")},
            {"easy", tr("difficulty.easy")},
            {"normal", tr("difficulty.normal")},
            {"hard", tr("difficulty.hard")}
    }, mValues["difficulty"]);
    accept(tr("server.difficulty.set", {paint(tr("difficulty." + mValues["difficulty"]), BOLD)}));
    writeLine();

    mValues["max-players"] = std::to_string(askInteger(tr("server.maxPlayers"),
                                                       std::atoi(mValues["max-players"].c_str()), 1, 1000));
    accept(tr("server.maxPlayers.set", {paint(mValues["max-players"], BOLD)}));
}

void SetupWizard::configureNetwork() {
    step(tr("network.title"), tr("network.description"));

    mValues["server-port"] = std::to_string(askInteger(tr("network.portV4"),
                                                       std::atoi(mValues["server-port"].c_str()), 1, 65535));
    mValues["server-portv6"] = std::to_string(askInteger(tr("network.portV6"),
                                                         std::atoi(mValues["server-portv6"].c_str()), 1, 65535));
    accept(tr("network.ports.set", {paint(mValues["server-port"], BOLD), paint(mValues["server-portv6"], BOLD)}));
    writeLine();

    mValues["transport"] = askChoice(tr("network.transport"), {
            {"raknet", tr("network.transport.raknet")},
            {"nethernet", tr("network.transport.nethernet")}
    }, mValues["transport"]);
    accept(tr("network.transport.set", {paint(mValues["transport"], BOLD)}));
    writeLine();

    const bool onlineMode = askConfirmation(tr("network.onlineMode"), true);
    mValues["online-mode"] = onlineMode ? "true" : "false";

    if (onlineMode)
        accept(tr("network.onlineMode.enabled"));
    else
        warning(tr("network.onlineMode.disabled"));
    writeLine();

    const bool lan = askConfirmation(tr("network.lan"), true);
    mValues["enable-lan-visibility"] = lan ? "true" : "false";
    accept(tr(lan ? "network.lan.enabled" : "network.lan.disabled"));
}

void SetupWizard::configureWorld() {
    step(tr("world.title"), tr("world.description"));

    mValues["level-name"] = ask(tr("world.name"), mValues["level-name"]);
    accept(tr("world.name.set", {paint(mValues["level-name"], BOLD)}));
    writeLine();

    hint(tr("world.seed.hint"));
    mValues["level-seed"] = ask(tr("world.seed"), mValues["level-seed"]);
    accept(mValues["level-seed"].empty() ? tr("world.seed.random")
                                         : tr("world.seed.set", {paint(mValues["level-seed"], BOLD)}));
    writeLine();

    mValues["view-distance"] = std::to_string(askInteger(tr("world.viewDistance"),
                                                         std::atoi(mValues["view-distance"].c_str()), 4, 96));
    accept(tr("world.viewDistance.set", {paint(mValues["view-distance"], BOLD)}));
}

void SetupWizard::configurePlayers() {
    step(tr("players.title"), tr("players.description"));

    hint(tr("players.operators.hint"));
    mOperators = askNames(tr("players.operators"));

    if (mOperators.empty())
        warning(tr("players.operators.none"));
    else
        accept(tr("players.operators.set", {paint(join(mOperators, ", "), BOLD)}));
    writeLine();

    const bool allowList = askConfirmation(tr("players.allowList"), false);
    mValues["allow-list"] = allowList ? "true" : "false";

    if (!allowList) {
        accept(tr("players.allowList.disabled"));
        return;
    }

    std::vector<std::string> invited = mOperators;
    hint(tr("players.allowList.hint"));

    for (const std::string &name: askNames(tr("players.allowList.players"))) {
        if (std::find(invited.begin(), invited.end(), name) == invited.end())
            invited.push_back(name);
    }

    mAllowedPlayers = invited;

    if (mAllowedPlayers.empty())
        warning(tr("players.allowList.empty"));
    else
        accept(tr("players.allowList.set", {paint(join(mAllowedPlayers, ", "), BOLD)}));
}

bool SetupWizard::save() {
    std::ofstream file(mPropertiesPath);
    if (!file.is_open()) {
        warning(tr("saveFailed", {mPropertiesPath}));
        return false;
    }

    for (const PropertyDefinition &definition: PropertiesSettings::getDefinitions()) {
        const auto it = mValues.find(definition.mKey);
        file << definition.mKey << "=" << (it == mValues.end() ? definition.mDefault : it->second) << "\n";
    }

    file.close();

    if (!mOperators.empty()) {
        OpList ops(mOpsPath);

        for (const std::string &name: mOperators)
            ops.addOp(name);
    }

    if (!mAllowedPlayers.empty()) {
        AllowList allowList(mAllowListPath);

        for (const std::string &name: mAllowedPlayers)
            allowList.add(name);
    }

    return true;
}

void SetupWizard::printAddresses() const {
    notice(tr("addresses.lookup"));

    const std::string port = mValues.at("server-port");
    const std::string local = findLocalAddress();
    const std::string external = findExternalAddress();
    const std::string localLabel = tr("addresses.local");
    const std::string externalLabel = tr("addresses.external");
    const size_t width = (std::max)(displayWidth(localLabel), displayWidth(externalLabel)) + 2;

    writeLine();
    writeLine("    " + paint(padRight(localLabel, width), DIM)
              + paint((local.empty() ? tr("addresses.unknown") : local) + ":" + port, BOLD));
    writeLine("    " + paint(padRight(externalLabel, width), DIM)
              + paint((external.empty() ? tr("addresses.unknown") : external) + ":" + port, BOLD));
    writeLine();
    hint(tr("addresses.hint"));
}

void SetupWizard::printSummary() const {
    writeLine();
    writeLine(paint(border(), ACCENT));
    writeLine(paint(center(tr("summary.title"), FRAME_WIDTH), BOLD));
    writeLine(paint(border(), ACCENT));
    writeLine();

    const std::vector<std::pair<std::string, std::string>> rows = {
            {tr("summary.language"),   Localization::getInstance().getLanguageName(mLocale)},
            {tr("summary.serverName"), mValues.at("server-name")},
            {tr("summary.gamemode"),   tr("gamemode." + mValues.at("gamemode")) + ", "
                                       + tr("difficulty." + mValues.at("difficulty"))},
            {tr("summary.players"),    tr("summary.players.value", {mValues.at("max-players")})},
            {tr("summary.ports"),      mValues.at("server-port") + " / " + mValues.at("server-portv6")},
            {tr("summary.transport"),  mValues.at("transport")},
            {tr("summary.onlineMode"), tr(mValues.at("online-mode") == "true" ? "summary.required"
                                                                              : "summary.notRequired")},
            {tr("summary.world"),      mValues.at("level-name")},
            {tr("summary.operators"),  mOperators.empty() ? tr("summary.none") : join(mOperators, ", ")},
            {tr("summary.allowList"),  tr(mValues.at("allow-list") == "true" ? "summary.on" : "summary.off")}
    };

    size_t width = 0;
    for (const auto &row: rows)
        width = (std::max)(width, displayWidth(row.first));

    for (const auto &row: rows)
        writeLine("    " + paint(padRight(row.first, width + 2), DIM) + row.second);

    writeLine();
    printAddresses();
    writeLine();
    notice(tr("summary.saved", {paint(mPropertiesPath, BOLD)}));
    writeLine();
}

bool SetupWizard::run(bool licenseAccepted, const std::string &language) {
    prepareConsole();

    for (const PropertyDefinition &definition: PropertiesSettings::getDefinitions())
        mValues[definition.mKey] = definition.mDefault;

    banner();
    selectLanguage(language);
    mValues["language"] = mLocale;

    if (!acceptLicense(licenseAccepted))
        return false;

    writeLine();
    notice(tr("welcome"));
    notice(tr("welcome.defaults", {paint("Enter", BOLD)}));
    hint(tr("welcome.flags"));
    writeLine();

    if (!askConfirmation(tr("configureNow"), true)) {
        accept(tr("keepDefaults"));
        return save();
    }

    configureServer();
    configureNetwork();
    configureWorld();
    configurePlayers();

    if (!save())
        return false;

    printSummary();
    ask(tr("pressEnter"), "");
    writeLine();
    return true;
}
