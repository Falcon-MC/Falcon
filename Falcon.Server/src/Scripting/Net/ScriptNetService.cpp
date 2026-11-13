#include "Scripting/Net/ScriptNetService.h"

ScriptNetService::~ScriptNetService() {
    for (std::unique_ptr<Job> &job: mJobs)
        job->mStream.abort();
    for (std::unique_ptr<Job> &job: mJobs) {
        if (job->mThread.joinable())
            job->mThread.join();
    }
    mSockets.clear();
}

void ScriptNetService::startRequest(uint32_t id, HttpsClient::Request request) {
    mJobs.push_back(std::make_unique<Job>());
    Job *job = mJobs.back().get();
    job->mId = id;
    job->mThread = std::thread([job, request]() {
        job->mResponse = HttpsClient::perform(request, job->mStream);
        job->mStream.close();
        job->mDone.store(true);
    });
}

std::vector<ScriptNetService::Completed> ScriptNetService::pollRequests() {
    std::vector<Completed> completed;
    for (auto it = mJobs.begin(); it != mJobs.end();) {
        Job &job = **it;
        if (!job.mDone.load()) {
            ++it;
            continue;
        }
        job.mThread.join();
        Completed done;
        done.mId = job.mId;
        done.mResponse = std::move(job.mResponse);
        completed.push_back(std::move(done));
        it = mJobs.erase(it);
    }
    return completed;
}

WebSocketClient &ScriptNetService::openSocket(uint32_t id, const std::string &url,
                                              const HttpsClient::Headers &headers, uint64_t maxMessageBytes) {
    std::unique_ptr<WebSocketClient> &socket = mSockets[id];
    socket = std::make_unique<WebSocketClient>();
    socket->setMaxMessageBytes(maxMessageBytes);
    socket->start(url, headers);
    return *socket;
}

WebSocketClient *ScriptNetService::findSocket(uint32_t id) {
    const auto it = mSockets.find(id);
    return it == mSockets.end() ? nullptr : it->second.get();
}

void ScriptNetService::removeSocket(uint32_t id) {
    mSockets.erase(id);
    mSocketUris.erase(id);
    mReceived.erase(id);
    mBacklog.erase(id);
}
