#include "Core/Debug/FileLogEndPoint.h"

FileLogEndPoint::FileLogEndPoint(const std::string &filePath) : mFilePath(filePath) {
    mStream.open(filePath, std::ios::out | std::ios::app);
}

FileLogEndPoint::~FileLogEndPoint() {
    if (mStream.is_open())
        mStream.close();
}

void FileLogEndPoint::log(const LogDetails &details) {
    if (!mStream.is_open())
        return;

    std::lock_guard<std::mutex> guard(mMutex);

    mStream << "[" << details.getTimestamp() << " " << toString(details.mLevel) << "] "
            << details.mMessage << "\n";
}

void FileLogEndPoint::flush() {
    if (!mStream.is_open())
        return;

    std::lock_guard<std::mutex> guard(mMutex);
    mStream.flush();
}
