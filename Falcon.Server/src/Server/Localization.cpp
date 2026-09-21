#include "Server/Localization.h"

#include "LanguageFiles.h"

#include <cctype>

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

const Localization &Localization::getInstance() {
    static const Localization instance;
    return instance;
}

Localization::Localization() {
    for (const FalconLanguageData::EmbeddedLanguage &language: FalconLanguageData::kLanguages)
        mLanguages[language.mLocale] = _parse(language.mContent);
}

std::string Localization::translate(const std::string &locale, const std::string &key,
                                    const std::vector<std::string> &parameters) const {
    const std::string *text = _findText(locale, key);
    if (text == nullptr)
        text = _findText(DEFAULT_LOCALE, key);

    return _format(text == nullptr ? key : *text, parameters);
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

std::string Localization::_format(const std::string &text, const std::vector<std::string> &parameters) {
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

        if (text[index + 1] == 's') {
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

        if (cursor > index + 1 && cursor + 1 < text.size() && text[cursor] == '$' && text[cursor + 1] == 's'
            && number >= 1) {
            if (number <= parameters.size())
                result += parameters[number - 1];
            index = cursor + 1;
            continue;
        }

        result += text[index];
    }

    return result;
}
