#pragma once

#include <string>

class SignedChat {
public:
    std::string mKeyId;
    std::string mSenderId;
    std::string mMessageId;
    std::string mScanText;
    std::string mRedactedText;
    std::string mAuthorName;
    std::string mSignature;
};

// server decoration around the signed name and message; outside the signature
class ChatFormatting {
public:
    struct Part {
        bool mPresent = false;
        std::string mValue;
    };

    Part mNamePrefix;
    Part mNamePostfix;
    Part mMessagePrefix;
    Part mMessagePostfix;
};
