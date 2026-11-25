#include "Network/Crypto/Base64.h"

#include <cstdint>

namespace {

    const char STANDARD_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const char URL_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    std::string encodeWith(const std::string &data, const char *alphabet, bool pad) {
        std::string out;
        out.reserve((data.size() + 2) / 3 * 4);

        size_t index = 0;
        while (index + 3 <= data.size()) {
            const uint32_t chunk = ((uint32_t) (uint8_t) data[index] << 16) |
                                   ((uint32_t) (uint8_t) data[index + 1] << 8) |
                                   (uint32_t) (uint8_t) data[index + 2];
            out.push_back(alphabet[(chunk >> 18) & 63]);
            out.push_back(alphabet[(chunk >> 12) & 63]);
            out.push_back(alphabet[(chunk >> 6) & 63]);
            out.push_back(alphabet[chunk & 63]);
            index += 3;
        }

        const size_t remaining = data.size() - index;
        if (remaining == 1) {
            const uint32_t chunk = (uint32_t) (uint8_t) data[index] << 16;
            out.push_back(alphabet[(chunk >> 18) & 63]);
            out.push_back(alphabet[(chunk >> 12) & 63]);
            if (pad)
                out.append("==");
        } else if (remaining == 2) {
            const uint32_t chunk = ((uint32_t) (uint8_t) data[index] << 16) |
                                   ((uint32_t) (uint8_t) data[index + 1] << 8);
            out.push_back(alphabet[(chunk >> 18) & 63]);
            out.push_back(alphabet[(chunk >> 12) & 63]);
            out.push_back(alphabet[(chunk >> 6) & 63]);
            if (pad)
                out.push_back('=');
        }

        return out;
    }

    int decodeCharacter(char character, bool url) {
        if (character >= 'A' && character <= 'Z')
            return character - 'A';
        if (character >= 'a' && character <= 'z')
            return character - 'a' + 26;
        if (character >= '0' && character <= '9')
            return character - '0' + 52;
        if (character == (url ? '-' : '+'))
            return 62;
        if (character == (url ? '_' : '/'))
            return 63;
        return -1;
    }

    bool decodeWith(const std::string &text, bool url, std::string &out) {
        out.clear();
        out.reserve(text.size() * 3 / 4);

        uint32_t buffer = 0;
        int bits = 0;
        for (const char character: text) {
            if (character == '=')
                break;

            const int value = decodeCharacter(character, url);
            if (value < 0)
                return false;

            buffer = (buffer << 6) | (uint32_t) value;
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                out.push_back((char) ((buffer >> bits) & 0xFF));
            }
        }
        return true;
    }

}

std::string Base64::encode(const std::string &data) {
    return encodeWith(data, STANDARD_ALPHABET, true);
}

bool Base64::decode(const std::string &text, std::string &out) {
    return decodeWith(text, false, out);
}

std::string Base64::encodeUrl(const std::string &data) {
    return encodeWith(data, URL_ALPHABET, false);
}

bool Base64::decodeUrl(const std::string &text, std::string &out) {
    return decodeWith(text, true, out);
}
