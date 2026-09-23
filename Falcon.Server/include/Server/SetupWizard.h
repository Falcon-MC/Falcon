#pragma once

#include <string>
#include <unordered_map>
#include <vector>

class SetupWizard {
public:
    SetupWizard(const std::string &propertiesPath, const std::string &opsPath, const std::string &allowListPath);

    static bool isNeeded(const std::string &propertiesPath);

    static bool isInteractive();

    bool run(bool licenseAccepted, const std::string &language);

private:
    struct Choice {
        std::string mValue;
        std::string mLabel;
    };

    void prepareConsole();

    std::string tr(const std::string &key, const std::vector<std::string> &parameters = {}) const;

    std::string paint(const std::string &text, const char *color) const;

    void writeLine(const std::string &line = "") const;

    void notice(const std::string &text) const;

    void accept(const std::string &text) const;

    void warning(const std::string &text) const;

    void hint(const std::string &text) const;

    void banner() const;

    void section(const std::string &label, const std::string &title) const;

    void step(const std::string &title, const std::string &description);

    void selectLanguage(const std::string &language);

    bool acceptLicense(bool licenseAccepted) const;

    std::string border() const;

    std::string separator() const;

    std::string readLine() const;

    std::string ask(const std::string &prompt, const std::string &defaultValue) const;

    bool askConfirmation(const std::string &prompt, bool defaultValue) const;

    int askInteger(const std::string &prompt, int defaultValue, int minimum, int maximum) const;

    void printChoices(const std::vector<Choice> &choices, const std::string &defaultValue) const;

    std::string askChoice(const std::string &prompt, const std::vector<Choice> &choices,
                          const std::string &defaultValue) const;

    std::vector<std::string> askNames(const std::string &prompt) const;

    void configureServer();

    void configureNetwork();

    void configureWorld();

    void configurePlayers();

    bool save();

    void printSummary() const;

    void printAddresses() const;

    std::string mPropertiesPath;
    std::string mOpsPath;
    std::string mAllowListPath;
    std::string mLocale;
    std::unordered_map<std::string, std::string> mValues;
    std::vector<std::string> mOperators;
    std::vector<std::string> mAllowedPlayers;
    bool mUseColor = false;
    bool mUseUnicode = false;
    int mStep = 0;
};
