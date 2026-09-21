#pragma once

#include <string>
#include <unordered_map>
#include <vector>

class Localization {
public:
    static constexpr const char *DEFAULT_LOCALE = "en_US";

    static const Localization &getInstance();

    std::string translate(const std::string &locale, const std::string &key,
                          const std::vector<std::string> &parameters = {}) const;

private:
    using Entries = std::unordered_map<std::string, std::string>;

    Localization();

    const Entries *_findEntries(const std::string &locale) const;

    const std::string *_findText(const std::string &locale, const std::string &key) const;

    static Entries _parse(const std::string &content);

    static std::string _format(const std::string &text, const std::vector<std::string> &parameters);

    std::unordered_map<std::string, Entries> mLanguages;
};
