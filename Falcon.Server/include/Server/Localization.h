#pragma once

#include <string>
#include <unordered_map>
#include <vector>

class Localization {
public:
    static constexpr const char *DEFAULT_LOCALE = "en_US";

    static constexpr const char *LANGUAGE_NAME_KEY = "language.name";

    static const Localization &getInstance();

    static void setServerLocale(const std::string &locale);

    static const std::string &getServerLocale();

    std::string translate(const std::string &locale, const std::string &key,
                          const std::vector<std::string> &parameters = {}) const;

    std::string translate(const std::string &key, const std::vector<std::string> &parameters = {}) const;

    bool hasLocale(const std::string &locale) const;

    std::vector<std::string> getLocales() const;

    std::string getLanguageName(const std::string &locale) const;

private:
    using Entries = std::unordered_map<std::string, std::string>;

    Localization();

    const Entries *_findEntries(const std::string &locale) const;

    const std::string *_findText(const std::string &locale, const std::string &key) const;

    static Entries _parse(const std::string &content);

    std::string _format(const std::string &locale, const std::string &text,
                        const std::vector<std::string> &parameters) const;

    std::string _inlineKey(const std::string &locale, const std::string &text, size_t start, size_t &end) const;

    std::unordered_map<std::string, Entries> mLanguages;

    static std::string sServerLocale;
};
