#pragma once

#include <string>
#include <unordered_map>
#include <vector>

class SetupWizard {
public:
    SetupWizard(const std::string &propertiesPath, const std::string &opsPath);

    static bool isNeeded(const std::string &propertiesPath);

    static bool isInteractive();

    bool run();

private:
    void writeLine(const std::string &line) const;

    void message(const std::string &text) const;

    void warning(const std::string &text) const;

    void frame(const std::string &title) const;

    std::string readLine() const;

    std::string ask(const std::string &prompt, const std::string &defaultValue) const;

    bool askConfirmation(const std::string &prompt, bool defaultValue) const;

    int askInteger(const std::string &prompt, int defaultValue, int minimum, int maximum) const;

    std::string askChoice(const std::string &prompt, const std::vector<std::string> &choices,
                          const std::string &defaultValue) const;

    void configureServer();

    void configureNetwork();

    void configureWorld();

    void configurePlayers();

    bool save();

    void printSummary() const;

    std::string mPropertiesPath;
    std::string mOpsPath;
    std::unordered_map<std::string, std::string> mValues;
    std::string mOperator;
};
