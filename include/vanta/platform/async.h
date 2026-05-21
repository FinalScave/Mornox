#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace vanta {

class CancellationToken {
public:
    CancellationToken();
    explicit CancellationToken(std::shared_ptr<std::atomic_bool> cancelled);

    bool Cancelled() const;

private:
    std::shared_ptr<std::atomic_bool> cancelled_;
};

class CancellationSource {
public:
    CancellationSource();

    CancellationToken Token() const;
    void Cancel();

private:
    std::shared_ptr<std::atomic_bool> cancelled_;
};

struct ProgressEvent {
    std::uint64_t job_id = 0;
    std::string title;
    std::string message;
    double fraction = -1.0;
};

class ProgressReporter {
public:
    using Sink = std::function<void(const ProgressEvent&)>;

    explicit ProgressReporter(Sink sink = {});

    void Report(ProgressEvent event) const;

private:
    Sink sink_;
};

class AsyncRuntime {
public:
    explicit AsyncRuntime(std::size_t worker_count = 0);
    AsyncRuntime(const AsyncRuntime&) = delete;
    AsyncRuntime& operator=(const AsyncRuntime&) = delete;
    ~AsyncRuntime();

    void Start(std::size_t worker_count = 0);
    void Stop();
    void PostWorker(std::function<void()> task);
    void PostMain(std::function<void()> task);
    std::size_t DrainMain();

private:
    void WorkerLoop();

    bool stopping_ = false;
    std::mutex worker_mutex_;
    std::condition_variable worker_condition_;
    std::queue<std::function<void()>> worker_queue_;
    std::vector<std::thread> workers_;

    std::mutex main_mutex_;
    std::queue<std::function<void()>> main_queue_;
};

}
