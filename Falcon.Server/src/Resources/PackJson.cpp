#include "Resources/PackJson.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace {
    // deeper documents are refused instead of overflowing the stack; packs never come close
    constexpr int MAX_DEPTH = 2048;

    class Reader {
    public:
        Reader(const char *begin, const char *end) : mCursor(begin), mEnd(end) {}

        bool parseDocument(PackJson::Value &out) {
            if (!skipWhitespaceAndComments() || peek() == '\0')
                return false;
            if (!parseValue(out, 0) || !skipWhitespaceAndComments())
                return false;
            // a NUL byte ends the document like the end of the file
            return peek() == '\0';
        }

    private:
        char peek() const {
            return mCursor == mEnd ? '\0' : *mCursor;
        }

        char take() {
            return mCursor == mEnd ? '\0' : *mCursor++;
        }

        bool consume(char expected) {
            if (peek() != expected)
                return false;
            ++mCursor;
            return true;
        }

        static bool isDigit(char c) {
            return c >= '0' && c <= '9';
        }

        void skipWhitespace() {
            for (char c = peek(); c == ' ' || c == '\n' || c == '\r' || c == '\t'; c = peek())
                ++mCursor;
        }

        bool skipWhitespaceAndComments() {
            skipWhitespace();
            while (consume('/')) {
                if (consume('*')) {
                    for (;;) {
                        if (peek() == '\0')
                            return false;
                        if (consume('*')) {
                            if (consume('/'))
                                break;
                        } else {
                            take();
                        }
                    }
                } else if (consume('/')) {
                    while (peek() != '\0' && take() != '\n') {
                    }
                } else {
                    return false;
                }
                skipWhitespace();
            }
            return true;
        }

        bool parseValue(PackJson::Value &out, int depth) {
            if (depth > MAX_DEPTH)
                return false;
            switch (peek()) {
                case 'n':
                    take();
                    out.mType = PackJson::Value::Type::Null;
                    return consume('u') && consume('l') && consume('l');
                case 't':
                    take();
                    out.mType = PackJson::Value::Type::True;
                    return consume('r') && consume('u') && consume('e');
                case 'f':
                    take();
                    out.mType = PackJson::Value::Type::False;
                    return consume('a') && consume('l') && consume('s') && consume('e');
                case '"':
                    out.mType = PackJson::Value::Type::String;
                    return parseString(out.mString);
                case '{':
                    return parseObject(out, depth);
                case '[':
                    return parseArray(out, depth);
                default:
                    return parseNumber(out);
            }
        }

        bool parseObject(PackJson::Value &out, int depth) {
            take();
            out.mType = PackJson::Value::Type::Object;
            if (!skipWhitespaceAndComments())
                return false;
            if (consume('}'))
                return true;
            for (;;) {
                if (peek() != '"')
                    return false;
                std::pair<std::string, PackJson::Value> member;
                if (!parseString(member.first) || !skipWhitespaceAndComments() || !consume(':')
                    || !skipWhitespaceAndComments() || !parseValue(member.second, depth + 1)
                    || !skipWhitespaceAndComments())
                    return false;
                out.mMembers.push_back(std::move(member));
                if (consume(',')) {
                    if (!skipWhitespaceAndComments())
                        return false;
                    continue;
                }
                return consume('}');
            }
        }

        bool parseArray(PackJson::Value &out, int depth) {
            take();
            out.mType = PackJson::Value::Type::Array;
            if (!skipWhitespaceAndComments())
                return false;
            if (consume(']'))
                return true;
            for (;;) {
                PackJson::Value element;
                if (!parseValue(element, depth + 1) || !skipWhitespaceAndComments())
                    return false;
                out.mElements.push_back(std::move(element));
                if (consume(',')) {
                    if (!skipWhitespaceAndComments())
                        return false;
                    continue;
                }
                return consume(']');
            }
        }

        bool parseHex4(uint32_t &codepoint) {
            codepoint = 0;
            for (int index = 0; index < 4; ++index) {
                const char c = peek();
                codepoint <<= 4;
                if (c >= '0' && c <= '9')
                    codepoint += (uint32_t) (c - '0');
                else if (c >= 'A' && c <= 'F')
                    codepoint += (uint32_t) (c - 'A' + 10);
                else if (c >= 'a' && c <= 'f')
                    codepoint += (uint32_t) (c - 'a' + 10);
                else
                    return false;
                take();
            }
            return true;
        }

        static void encodeUtf8(uint32_t codepoint, std::string &out) {
            if (codepoint <= 0x7F) {
                out += (char) codepoint;
            } else if (codepoint <= 0x7FF) {
                out += (char) (0xC0 | ((codepoint >> 6) & 0xFF));
                out += (char) (0x80 | (codepoint & 0x3F));
            } else if (codepoint <= 0xFFFF) {
                out += (char) (0xE0 | ((codepoint >> 12) & 0xFF));
                out += (char) (0x80 | ((codepoint >> 6) & 0x3F));
                out += (char) (0x80 | (codepoint & 0x3F));
            } else {
                out += (char) (0xF0 | ((codepoint >> 18) & 0xFF));
                out += (char) (0x80 | ((codepoint >> 12) & 0x3F));
                out += (char) (0x80 | ((codepoint >> 6) & 0x3F));
                out += (char) (0x80 | (codepoint & 0x3F));
            }
        }

        // no validation of the bytes: invalid UTF-8 and a lone low surrogate go through as the game lets them
        bool parseString(std::string &out) {
            take();
            for (;;) {
                const char c = peek();
                if (c == '\\') {
                    take();
                    const char escaped = peek();
                    char replacement = 0;
                    switch (escaped) {
                        case '"':
                            replacement = '"';
                            break;
                        case '/':
                            replacement = '/';
                            break;
                        case '\\':
                            replacement = '\\';
                            break;
                        case 'b':
                            replacement = '\b';
                            break;
                        case 'f':
                            replacement = '\f';
                            break;
                        case 'n':
                            replacement = '\n';
                            break;
                        case 'r':
                            replacement = '\r';
                            break;
                        case 't':
                            replacement = '\t';
                            break;
                        default:
                            break;
                    }
                    if (replacement != 0) {
                        take();
                        out += replacement;
                        continue;
                    }
                    if (escaped != 'u')
                        return false;
                    take();
                    uint32_t codepoint = 0;
                    if (!parseHex4(codepoint))
                        return false;
                    if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                        uint32_t low = 0;
                        if (!consume('\\') || !consume('u') || !parseHex4(low) || low < 0xDC00 || low > 0xDFFF)
                            return false;
                        codepoint = (((codepoint - 0xD800) << 10) | (low - 0xDC00)) + 0x10000;
                    }
                    encodeUtf8(codepoint, out);
                } else if (c == '"') {
                    take();
                    return true;
                } else if ((unsigned char) c < 0x20) {
                    // unescaped control characters and NUL are refused
                    return false;
                } else {
                    out += take();
                }
            }
        }

        // two roundings like the game: a fused multiply-add would round once and change the last bit
        static double timesTenPlus(double value, int digit) {
            volatile double product = value * 10.0;
            return product + digit;
        }

        static const double *powersOfTen() {
            static const double *table = [] {
                static double values[309];
                for (int exponent = 0; exponent <= 308; ++exponent)
                    values[exponent] = std::strtod(("1e" + std::to_string(exponent)).c_str(), nullptr);
                return values;
            }();
            return table;
        }

        static double fastPath(double significand, int exponent) {
            if (exponent < -308)
                return 0.0;
            if (exponent >= 0)
                return significand * powersOfTen()[exponent];
            return significand / powersOfTen()[-exponent];
        }

        // the fast decimal conversion: a significand of at most 17 digits scaled by a power of ten, so the last
        // bit can differ from a correctly rounded parse
        static double strtodNormalPrecision(double significand, int exponent) {
            if (exponent < -308) {
                significand = fastPath(significand, -308);
                return fastPath(significand, exponent + 308);
            }
            return fastPath(significand, exponent);
        }

        bool parseNumber(PackJson::Value &out) {
            const bool minus = consume('-');

            uint32_t i = 0;
            uint64_t i64 = 0;
            bool use64Bit = false;
            int significandDigit = 0;
            if (peek() == '0') {
                take();
            } else if (peek() >= '1' && peek() <= '9') {
                i = (uint32_t) (take() - '0');
                const uint32_t limit = minus ? 214748364u : 429496729u;
                const char lastDigit = minus ? '8' : '5';
                while (isDigit(peek())) {
                    if (i >= limit && (i != limit || peek() > lastDigit)) {
                        i64 = i;
                        use64Bit = true;
                        break;
                    }
                    i = i * 10 + (uint32_t) (take() - '0');
                    ++significandDigit;
                }
            } else {
                return false;
            }

            bool useDouble = false;
            double d = 0.0;
            if (use64Bit) {
                const uint64_t limit = minus ? 0x0CCCCCCCCCCCCCCCull : 0x1999999999999999ull;
                const char lastDigit = minus ? '8' : '5';
                while (isDigit(peek())) {
                    if (i64 >= limit && (i64 != limit || peek() > lastDigit)) {
                        d = (double) i64;
                        useDouble = true;
                        break;
                    }
                    i64 = i64 * 10 + (uint64_t) (take() - '0');
                    ++significandDigit;
                }
            }
            if (useDouble) {
                while (isDigit(peek()))
                    d = timesTenPlus(d, take() - '0');
            }

            int expFrac = 0;
            if (consume('.')) {
                if (!isDigit(peek()))
                    return false;
                if (!useDouble) {
                    if (!use64Bit)
                        i64 = i;
                    while (isDigit(peek())) {
                        // up to 2^53 - 1 the significand stays exact
                        if (i64 > 0x1FFFFFFFFFFFFFull)
                            break;
                        i64 = i64 * 10 + (uint64_t) (take() - '0');
                        --expFrac;
                        if (i64 != 0)
                            ++significandDigit;
                    }
                    d = (double) i64;
                    useDouble = true;
                }
                while (isDigit(peek())) {
                    if (significandDigit < 17) {
                        d = timesTenPlus(d, take() - '0');
                        --expFrac;
                        if (d > 0.0)
                            ++significandDigit;
                    } else {
                        take();
                    }
                }
            }

            int exponent = 0;
            if (consume('e') || consume('E')) {
                if (!useDouble) {
                    d = use64Bit ? (double) i64 : (double) i;
                    useDouble = true;
                }
                bool expMinus = false;
                if (!consume('+'))
                    expMinus = consume('-');
                if (!isDigit(peek()))
                    return false;
                exponent = take() - '0';
                if (expMinus) {
                    while (isDigit(peek())) {
                        exponent = exponent * 10 + (take() - '0');
                        if (exponent >= 214748364) {
                            while (isDigit(peek()))
                                take();
                        }
                    }
                } else {
                    const int maxExponent = 308 - expFrac;
                    while (isDigit(peek())) {
                        exponent = exponent * 10 + (take() - '0');
                        if (exponent > maxExponent)
                            return false;
                    }
                }
                if (expMinus)
                    exponent = -exponent;
            }

            if (useDouble) {
                d = strtodNormalPrecision(d, exponent + expFrac);
                out.mType = PackJson::Value::Type::Double;
                out.mDouble = minus ? -d : d;
            } else if (use64Bit) {
                if (minus) {
                    out.mType = PackJson::Value::Type::Int64;
                    out.mInteger = (int64_t) (~i64 + 1);
                } else {
                    out.mType = PackJson::Value::Type::Uint64;
                    out.mUnsigned = i64;
                }
            } else if (minus) {
                out.mType = PackJson::Value::Type::Int;
                out.mInteger = (int32_t) (~i + 1);
            } else {
                out.mType = PackJson::Value::Type::Uint;
                out.mUnsigned = i;
            }
            return true;
        }

        const char *mCursor;
        const char *mEnd;
    };

    // Grisu2 as the game runs it: the closest-digit correction is skipped once the digits run 9 or more past the
    // decimal point, so a few doubles come out with a last digit still inside the rounding interval but not closest
    struct DiyFp {
        uint64_t f = 0;
        int e = 0;

        DiyFp() = default;

        DiyFp(uint64_t fraction, int exponent) : f(fraction), e(exponent) {}

        explicit DiyFp(double value) {
            uint64_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            const int biased = (int) ((bits >> 52) & 0x7FF);
            const uint64_t significand = bits & 0xFFFFFFFFFFFFFull;
            if (biased != 0) {
                f = significand + 0x10000000000000ull;
                e = biased - 1075;
            } else {
                f = significand;
                e = -1074;
            }
        }

        DiyFp operator-(const DiyFp &other) const {
            return {f - other.f, e};
        }

        DiyFp operator*(const DiyFp &other) const {
            const uint64_t mask = 0xFFFFFFFFull;
            const uint64_t a = f >> 32;
            const uint64_t b = f & mask;
            const uint64_t c = other.f >> 32;
            const uint64_t d = other.f & mask;
            const uint64_t ac = a * c;
            const uint64_t bc = b * c;
            const uint64_t ad = a * d;
            const uint64_t bd = b * d;
            uint64_t tmp = (bd >> 32) + (ad & mask) + (bc & mask);
            tmp += 1u << 31;
            return {ac + (ad >> 32) + (bc >> 32) + (tmp >> 32), e + other.e + 64};
        }

        DiyFp normalize() const {
            DiyFp result = *this;
            while (!(result.f & (1ull << 63))) {
                result.f <<= 1;
                --result.e;
            }
            return result;
        }

        DiyFp normalizeBoundary() const {
            DiyFp result = *this;
            while (!(result.f & (0x10000000000000ull << 1))) {
                result.f <<= 1;
                --result.e;
            }
            result.f <<= 10;
            result.e -= 10;
            return result;
        }

        void normalizedBoundaries(DiyFp &minus, DiyFp &plus) const {
            const DiyFp upper = DiyFp((f << 1) + 1, e - 1).normalizeBoundary();
            DiyFp lower = f == 0x10000000000000ull ? DiyFp((f << 2) - 1, e - 2) : DiyFp((f << 1) - 1, e - 1);
            lower.f <<= lower.e - upper.e;
            lower.e = upper.e;
            plus = upper;
            minus = lower;
        }
    };

    // normalized 10^-348, 10^-340, ..., 10^340
    const uint64_t CACHED_POWERS_F[] = {
            0xfa8fd5a0081c0288ull, 0xbaaee17fa23ebf76ull, 0x8b16fb203055ac76ull, 0xcf42894a5dce35eaull,
            0x9a6bb0aa55653b2dull, 0xe61acf033d1a45dfull, 0xab70fe17c79ac6caull, 0xff77b1fcbebcdc4full,
            0xbe5691ef416bd60cull, 0x8dd01fad907ffc3cull, 0xd3515c2831559a83ull, 0x9d71ac8fada6c9b5ull,
            0xea9c227723ee8bcbull, 0xaecc49914078536dull, 0x823c12795db6ce57ull, 0xc21094364dfb5637ull,
            0x9096ea6f3848984full, 0xd77485cb25823ac7ull, 0xa086cfcd97bf97f4ull, 0xef340a98172aace5ull,
            0xb23867fb2a35b28eull, 0x84c8d4dfd2c63f3bull, 0xc5dd44271ad3cdbaull, 0x936b9fcebb25c996ull,
            0xdbac6c247d62a584ull, 0xa3ab66580d5fdaf6ull, 0xf3e2f893dec3f126ull, 0xb5b5ada8aaff80b8ull,
            0x87625f056c7c4a8bull, 0xc9bcff6034c13053ull, 0x964e858c91ba2655ull, 0xdff9772470297ebdull,
            0xa6dfbd9fb8e5b88full, 0xf8a95fcf88747d94ull, 0xb94470938fa89bcfull, 0x8a08f0f8bf0f156bull,
            0xcdb02555653131b6ull, 0x993fe2c6d07b7facull, 0xe45c10c42a2b3b06ull, 0xaa242499697392d3ull,
            0xfd87b5f28300ca0eull, 0xbce5086492111aebull, 0x8cbccc096f5088ccull, 0xd1b71758e219652cull,
            0x9c40000000000000ull, 0xe8d4a51000000000ull, 0xad78ebc5ac620000ull, 0x813f3978f8940984ull,
            0xc097ce7bc90715b3ull, 0x8f7e32ce7bea5c70ull, 0xd5d238a4abe98068ull, 0x9f4f2726179a2245ull,
            0xed63a231d4c4fb27ull, 0xb0de65388cc8ada8ull, 0x83c7088e1aab65dbull, 0xc45d1df942711d9aull,
            0x924d692ca61be758ull, 0xda01ee641a708deaull, 0xa26da3999aef774aull, 0xf209787bb47d6b85ull,
            0xb454e4a179dd1877ull, 0x865b86925b9bc5c2ull, 0xc83553c5c8965d3dull, 0x952ab45cfa97a0b3ull,
            0xde469fbd99a05fe3ull, 0xa59bc234db398c25ull, 0xf6c69a72a3989f5cull, 0xb7dcbf5354e9beceull,
            0x88fcf317f22241e2ull, 0xcc20ce9bd35c78a5ull, 0x98165af37b2153dfull, 0xe2a0b5dc971f303aull,
            0xa8d9d1535ce3b396ull, 0xfb9b7cd9a4a7443cull, 0xbb764c4ca7a44410ull, 0x8bab8eefb6409c1aull,
            0xd01fef10a657842cull, 0x9b10a4e5e9913129ull, 0xe7109bfba19c0c9dull, 0xac2820d9623bf429ull,
            0x80444b5e7aa7cf85ull, 0xbf21e44003acdd2dull, 0x8e679c2f5e44ff8full, 0xd433179d9c8cb841ull,
            0x9e19db92b4e31ba9ull, 0xeb96bf6ebadf77d9ull, 0xaf87023b9bf0ee6bull};

    const int16_t CACHED_POWERS_E[] = {
            -1220, -1193, -1166, -1140, -1113, -1087, -1060, -1034, -1007, -980, -954, -927, -901, -874, -847,
            -821,  -794,  -768,  -741,  -715,  -688,  -661,  -635,  -608,  -582, -555, -529, -502, -475, -449,
            -422,  -396,  -369,  -343,  -316,  -289,  -263,  -236,  -210,  -183, -157, -130, -103, -77,  -50,
            -24,   3,     30,    56,    83,    109,   136,   162,   189,   216,  242,  269,  295,  322,  348,
            375,   402,   428,   455,   481,   508,   534,   561,   588,   614,  641,  667,  694,  720,  747,
            774,   800,   827,   853,   880,   907,   933,   960,   986,   1013, 1039, 1066};

    DiyFp cachedPower(int e, int &k) {
        const double dk = (-61 - e) * 0.30102999566398114 + 347;
        int rounded = (int) dk;
        if (dk - rounded > 0.0)
            ++rounded;
        const unsigned index = (unsigned) ((rounded >> 3) + 1);
        k = -(-348 + (int) (index << 3));
        return {CACHED_POWERS_F[index], CACHED_POWERS_E[index]};
    }

    void grisuRound(char *buffer, int length, uint64_t delta, uint64_t rest, uint64_t tenKappa, uint64_t wpW) {
        while (rest < wpW && delta - rest >= tenKappa
               && (rest + tenKappa < wpW || wpW - rest > rest + tenKappa - wpW)) {
            buffer[length - 1]--;
            rest += tenKappa;
        }
    }

    int countDecimalDigit32(uint32_t n) {
        if (n < 10)
            return 1;
        if (n < 100)
            return 2;
        if (n < 1000)
            return 3;
        if (n < 10000)
            return 4;
        if (n < 100000)
            return 5;
        if (n < 1000000)
            return 6;
        if (n < 10000000)
            return 7;
        if (n < 100000000)
            return 8;
        return 9;
    }

    void digitGen(const DiyFp &w, const DiyFp &mp, uint64_t delta, char *buffer, int &length, int &k) {
        static const uint32_t POW10[] = {1,      10,      100,      1000,      10000,
                                         100000, 1000000, 10000000, 100000000, 1000000000};
        const DiyFp one(1ull << -mp.e, mp.e);
        const DiyFp wpW = mp - w;
        uint32_t p1 = (uint32_t) (mp.f >> -one.e);
        uint64_t p2 = mp.f & (one.f - 1);
        int kappa = countDecimalDigit32(p1);
        length = 0;

        while (kappa > 0) {
            uint32_t digit = p1 / POW10[kappa - 1];
            p1 %= POW10[kappa - 1];
            if (digit || length)
                buffer[length++] = (char) ('0' + digit);
            --kappa;
            const uint64_t tmp = ((uint64_t) p1 << -one.e) + p2;
            if (tmp <= delta) {
                k += kappa;
                grisuRound(buffer, length, delta, tmp, (uint64_t) POW10[kappa] << -one.e, wpW.f);
                return;
            }
        }

        for (;;) {
            p2 *= 10;
            delta *= 10;
            const char digit = (char) (p2 >> -one.e);
            if (digit || length)
                buffer[length++] = (char) ('0' + digit);
            p2 &= one.f - 1;
            --kappa;
            if (p2 < delta) {
                k += kappa;
                const int index = -kappa;
                grisuRound(buffer, length, delta, p2, one.f, wpW.f * (index < 9 ? POW10[index] : 0));
                return;
            }
        }
    }

    void grisu2(double value, char *buffer, int &length, int &k) {
        const DiyFp v(value);
        DiyFp minus;
        DiyFp plus;
        v.normalizedBoundaries(minus, plus);

        const DiyFp cached = cachedPower(plus.e, k);
        const DiyFp w = v.normalize() * cached;
        DiyFp wp = plus * cached;
        DiyFp wm = minus * cached;
        wm.f++;
        wp.f--;
        digitGen(w, wp, wp.f - wm.f, buffer, length, k);
    }

    std::string prettify(const char *digits, int length, int k) {
        const int kk = length + k;
        const std::string text(digits, (size_t) length);
        if (k >= 0 && kk <= 21)
            return text + std::string((size_t) (kk - length), '0') + ".0";
        if (kk > 0 && kk <= 21)
            return text.substr(0, (size_t) kk) + "." + text.substr((size_t) kk);
        if (kk > -6 && kk <= 0)
            return "0." + std::string((size_t) -kk, '0') + text;
        if (length == 1)
            return text + "e" + std::to_string(kk - 1);
        return text.substr(0, 1) + "." + text.substr(1) + "e" + std::to_string(kk - 1);
    }

    void writeString(const std::string &value, std::string &out) {
        static const char HEX[] = "0123456789ABCDEF";
        out += '"';
        for (const char c: value) {
            const auto byte = (unsigned char) c;
            if (byte < 0x20) {
                out += '\\';
                switch (byte) {
                    case '\b':
                        out += 'b';
                        break;
                    case '\t':
                        out += 't';
                        break;
                    case '\n':
                        out += 'n';
                        break;
                    case '\f':
                        out += 'f';
                        break;
                    case '\r':
                        out += 'r';
                        break;
                    default:
                        out += "u00";
                        out += HEX[byte >> 4];
                        out += HEX[byte & 0xF];
                        break;
                }
            } else if (c == '"' || c == '\\') {
                out += '\\';
                out += c;
            } else {
                out += c;
            }
        }
        out += '"';
    }
}

PackJson::Value *PackJson::Value::find(std::string_view name) {
    for (std::pair<std::string, Value> &member: mMembers) {
        if (member.first == name)
            return &member.second;
    }
    return nullptr;
}

const PackJson::Value *PackJson::Value::find(std::string_view name) const {
    for (const std::pair<std::string, Value> &member: mMembers) {
        if (member.first == name)
            return &member.second;
    }
    return nullptr;
}

bool PackJson::parse(std::string_view text, Value &out) {
    const char *begin = text.data();
    const char *end = begin + text.size();
    // each byte of the mark is skipped on its own, in order
    for (const unsigned char mark: {0xEF, 0xBB, 0xBF}) {
        if (begin != end && (unsigned char) *begin == mark)
            ++begin;
    }
    out = Value();
    Reader reader(begin, end);
    return reader.parseDocument(out);
}

std::string PackJson::formatDouble(double value) {
    if (value == 0.0)
        return std::signbit(value) ? "-0.0" : "0.0";
    std::string sign;
    if (value < 0) {
        sign = "-";
        value = -value;
    }
    char digits[32];
    int length = 0;
    int k = 0;
    grisu2(value, digits, length, k);
    return sign + prettify(digits, length, k);
}

bool PackJson::write(const Value &value, std::string &out) {
    switch (value.mType) {
        case Value::Type::Null:
            out += "null";
            return true;
        case Value::Type::False:
            out += "false";
            return true;
        case Value::Type::True:
            out += "true";
            return true;
        case Value::Type::String:
            writeString(value.mString, out);
            return true;
        case Value::Type::Int:
        case Value::Type::Int64:
            out += std::to_string(value.mInteger);
            return true;
        case Value::Type::Uint:
        case Value::Type::Uint64:
            out += std::to_string(value.mUnsigned);
            return true;
        case Value::Type::Double:
            if (!std::isfinite(value.mDouble))
                return false;
            out += formatDouble(value.mDouble);
            return true;
        case Value::Type::Object: {
            out += '{';
            bool first = true;
            for (const std::pair<std::string, Value> &member: value.mMembers) {
                if (!first)
                    out += ',';
                first = false;
                writeString(member.first, out);
                out += ':';
                if (!write(member.second, out))
                    return false;
            }
            out += '}';
            return true;
        }
        case Value::Type::Array: {
            out += '[';
            bool first = true;
            for (const Value &element: value.mElements) {
                if (!first)
                    out += ',';
                first = false;
                if (!write(element, out))
                    return false;
            }
            out += ']';
            return true;
        }
    }
    return false;
}

bool PackJson::minify(std::string_view text, std::string &out) {
    Value document;
    if (!parse(text, document))
        return false;
    out.clear();
    return write(document, out);
}
