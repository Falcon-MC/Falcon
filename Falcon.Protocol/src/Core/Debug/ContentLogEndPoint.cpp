#include "Core/Debug/ContentLogEndPoint.h"

#include <cstdio>
#include <string>

#ifndef _WIN32

#include <unistd.h>

#endif

ContentLogEndPoint::ContentLogEndPoint(bool useColor) : mUseColor(useColor) {
#ifndef _WIN32
    if (mUseColor && isatty(fileno(stdout)) == 0)
        mUseColor = false;
#endif

    setvbuf(stdout, nullptr, _IOFBF, STDOUT_BUFFER_SIZE);
}

const char *ContentLogEndPoint::_getColorCode(LogLevel level) {
    switch (level) {
        case LogLevel::Verbose:
            return "\033[90m";
        case LogLevel::Warning:
            return "\033[33m";
        case LogLevel::Error:
            return "\033[31m";
        case LogLevel::Fatal:
            return "\033[1;31m";
        default:
            return "\033[37m";
    }
}

void ContentLogEndPoint::log(const LogDetails &details) {
    std::string line;
    line.reserve(details.mMessage.size() + LINE_PREFIX_RESERVE);

    if (mUseColor)
        line.append(_getColorCode(details.mLevel));

    line.append("[");
    line.append(details.getTimestamp());
    line.append(" ");
    line.append(toString(details.mLevel));
    line.append("]");

    if (mUseColor)
        line.append("\033[0m");

    line.append(" ");
    line.append(details.mMessage);
    line.append("\n");

    std::lock_guard<std::mutex> guard(mMutex);
    fwrite(line.data(), 1, line.size(), stdout);
}

void ContentLogEndPoint::flush() {
    std::lock_guard<std::mutex> guard(mMutex);
    fflush(stdout);
}
