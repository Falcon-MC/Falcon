#include "Server/Localization.h"

#include "CommandTranslationsJson.h"
#include "Core/Json/Json.h"
#include "LanguageFiles.h"

#include <algorithm>
#include <cctype>
#include <memory>

namespace {
    std::string trim(const std::string &value) {
        size_t start = 0;
        size_t end = value.size();

        while (start < end && std::isspace((unsigned char) value[start]))
            ++start;
        while (end > start && std::isspace((unsigned char) value[end - 1]))
            --end;

        return value.substr(start, end - start);
    }

    std::string languageOf(const std::string &locale) {
        const size_t separator = locale.find_first_of("_-");
        return separator == std::string::npos ? locale : locale.substr(0, separator);
    }
}

std::string Localization::sServerLocale = Localization::DEFAULT_LOCALE;

const Localization &Localization::getInstance() {
    static const Localization instance;
    return instance;
}

void Localization::setServerLocale(const std::string &locale) {
    sServerLocale = getInstance().hasLocale(locale) ? locale : std::string(DEFAULT_LOCALE);
}

const std::string &Localization::getServerLocale() {
    return sServerLocale;
}

std::string Localization::translate(const std::string &key, const std::vector<std::string> &parameters) const {
    return translate(sServerLocale, key, parameters);
}

bool Localization::hasLocale(const std::string &locale) const {
    return mLanguages.find(locale) != mLanguages.end();
}

std::vector<std::string> Localization::getLocales() const {
    std::vector<std::string> locales;
    locales.reserve(mLanguages.size());

    for (const auto &entry: mLanguages)
        locales.push_back(entry.first);

    std::sort(locales.begin(), locales.end());
    return locales;
}

std::string Localization::getLanguageName(const std::string &locale) const {
    const auto entries = mLanguages.find(locale);
    if (entries == mLanguages.end())
        return locale;

    const auto name = entries->second.find(LANGUAGE_NAME_KEY);
    return name == entries->second.end() ? locale : name->second;
}

Localization::Localization() {
    for (const FalconLanguageData::EmbeddedLanguage &language: FalconLanguageData::kLanguages)
        mLanguages[language.mLocale] = _parse(language.mContent);

    const std::unique_ptr<json::Value> vanilla = json::parse(FalconLanguageData::kCommandTranslationsJson);
    if (vanilla == nullptr || !vanilla->isObject())
        return;

    for (auto &language: mLanguages) {
        const json::Value *texts = vanilla->get(language.first);
        if (texts == nullptr || !texts->isObject())
            continue;

        for (const std::string &key: texts->mKeys)
            language.second.emplace(key, texts->get(key)->string());
    }
}

std::string Localization::translate(const std::string &locale, const std::string &key,
                                    const std::vector<std::string> &parameters) const {
    const std::string *text = _findText(locale, key);
    if (text == nullptr)
        text = _findText(DEFAULT_LOCALE, key);

    return _format(locale, text == nullptr ? key : *text, parameters);
}

std::string Localization::_inlineKey(const std::string &locale, const std::string &text, size_t start,
                                     size_t &end) const {
    end = start;
    while (end < text.size() && (std::isalnum((unsigned char) text[end]) || text[end] == '.' || text[end] == '_'
                                 || text[end] == '-'))
        ++end;

    while (end > start && text[end - 1] == '.')
        --end;

    if (end == start)
        return std::string();

    const std::string key = text.substr(start, end - start);
    const std::string *found = _findText(locale, key);
    if (found == nullptr)
        found = _findText(DEFAULT_LOCALE, key);
    return found == nullptr ? std::string() : *found;
}

const Localization::Entries *Localization::_findEntries(const std::string &locale) const {
    const auto exact = mLanguages.find(locale);
    if (exact != mLanguages.end())
        return &exact->second;

    const std::string language = languageOf(locale);
    for (const auto &entry: mLanguages) {
        if (languageOf(entry.first) == language)
            return &entry.second;
    }

    return nullptr;
}

const std::string *Localization::_findText(const std::string &locale, const std::string &key) const {
    const Entries *entries = _findEntries(locale);
    if (entries == nullptr)
        return nullptr;

    const auto text = entries->find(key);
    return text == entries->end() ? nullptr : &text->second;
}

Localization::Entries Localization::_parse(const std::string &content) {
    Entries entries;
    size_t position = 0;

    while (position < content.size()) {
        size_t end = content.find('\n', position);
        if (end == std::string::npos)
            end = content.size();

        std::string line = content.substr(position, end - position);
        position = end + 1;

        const size_t comment = line.find("\t#");
        if (comment != std::string::npos)
            line = line.substr(0, comment);

        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        const size_t separator = line.find('=');
        if (separator == std::string::npos)
            continue;

        entries[trim(line.substr(0, separator))] = line.substr(separator + 1);
    }

    return entries;
}

std::string Localization::_format(const std::string &locale, const std::string &text,
                                  const std::vector<std::string> &parameters) const {
    std::string result;
    result.reserve(text.size());
    size_t nextParameter = 0;

    for (size_t index = 0; index < text.size(); ++index) {
        if (text[index] != '%' || index + 1 >= text.size()) {
            result += text[index];
            continue;
        }

        if (text[index + 1] == '%') {
            result += '%';
            ++index;
            continue;
        }

        if (text[index + 1] == 's' || text[index + 1] == 'd') {
            if (nextParameter < parameters.size())
                result += parameters[nextParameter];
            ++nextParameter;
            ++index;
            continue;
        }

        size_t cursor = index + 1;
        size_t number = 0;
        while (cursor < text.size() && std::isdigit((unsigned char) text[cursor])) {
            number = number * 10 + (size_t) (text[cursor] - '0');
            ++cursor;
        }

        if (cursor > index + 1 && cursor + 1 < text.size() && text[cursor] == '$'
            && (text[cursor + 1] == 's' || text[cursor + 1] == 'd') && number >= 1) {
            if (number <= parameters.size())
                result += parameters[number - 1];
            index = cursor + 1;
            continue;
        }

        size_t keyEnd = 0;
        const std::string inlineText = _inlineKey(locale, text, index + 1, keyEnd);
        if (!inlineText.empty()) {
            result += inlineText;
            index = keyEnd - 1;
            continue;
        }

        result += text[index];
    }

    return result;
}
