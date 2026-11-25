#pragma once

#include "Core/Debug/ILogEndPoint.h"

#include <fstream>
#include <mutex>
#include <string>

class FileLogEndPoint : public ILogEndPoint {
public:
    explicit FileLogEndPoint(const std::string &filePath);

    ~FileLogEndPoint() override;

    bool isOpen() const { return mStream.is_open(); }

    const std::string &getFilePath() const { return mFilePath; }

    void log(const LogDetails &details) override;

    void flush() override;

private:
    std::string mFilePath;
    std::ofstream mStream;
    std::mutex mMutex;
};
