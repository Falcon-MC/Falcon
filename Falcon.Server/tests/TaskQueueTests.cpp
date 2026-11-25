#include "Core/Threading/TaskQueue.h"

#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }
}

int main() {
    TaskQueue<std::unique_ptr<int>> queue;
    std::vector<std::unique_ptr<int>> batch;
    batch.reserve(64);
    for (int i = 0; i < 64; ++i)
        batch.push_back(std::make_unique<int>(i));
    const size_t capacity = batch.capacity();
    queue.pushAll(std::move(batch));
    require(batch.empty() && batch.capacity() == capacity, "producer storage was not retained");
    auto first = queue.drain();
    require(first.size() == 64, "batch was lost");
    for (size_t i = 0; i < first.size(); ++i)
        require(*first[i] == (int) i, "batch order changed");

    constexpr int COUNT = 20000;
    std::atomic<bool> done = false;
    std::thread producer([&]() {
        for (int i = 0; i < COUNT; ++i) {
            batch.push_back(std::make_unique<int>(i));
            if (batch.size() == 32)
                queue.pushAll(std::move(batch));
        }
        queue.pushAll(std::move(batch));
        done.store(true);
    });
    int received = 0;
    while (!done.load() || !queue.empty()) {
        for (const auto &value: queue.drain()) {
            require(*value == received, "concurrent drain lost or reordered a value");
            ++received;
        }
        std::this_thread::yield();
    }
    producer.join();
    require(received == COUNT, "concurrent drain did not deliver every value");
    queue.push(std::make_unique<int>(COUNT));
    queue.close();
    std::unique_ptr<int> last;
    require(queue.waitPop(last) && *last == COUNT, "close discarded queued data");
    require(!queue.waitPop(last), "closed queue did not stop its consumer");
    batch.push_back(std::make_unique<int>(-1));
    queue.pushAll(std::move(batch));
    require(batch.empty() && queue.empty(), "closed queue accepted a batch");
}
