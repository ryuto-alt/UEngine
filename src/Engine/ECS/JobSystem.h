#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <vector>
#include <queue>
#include <atomic>
#include <cstdint>

namespace ECS {

class JobSystem {
public:
    using Job = std::function<void()>;

    static JobSystem& GetInstance() {
        static JobSystem s_instance;
        return s_instance;
    }

    void Initialize(uint32_t threadCount = 0) {
        if (m_running.load()) return;
        m_running.store(true);

        if (threadCount == 0) {
            threadCount = (std::max)(1u, std::thread::hardware_concurrency() - 1);
        }

        m_workers.reserve(threadCount);
        for (uint32_t i = 0; i < threadCount; ++i) {
            m_workers.emplace_back([this] { WorkerThread(); });
        }
    }

    void Shutdown() {
        m_running.store(false);
        m_condition.notify_all();
        m_workers.clear(); // jthread auto-joins
    }

    void Submit(Job job) {
        {
            std::lock_guard lock(m_mutex);
            m_jobQueue.push(std::move(job));
        }
        m_condition.notify_one();
    }

    // Submit multiple jobs and block until all complete
    void SubmitAndWait(std::vector<Job>& jobs) {
        if (jobs.empty()) return;

        std::atomic<uint32_t> remaining{ static_cast<uint32_t>(jobs.size()) };

        for (auto& job : jobs) {
            Submit([&remaining, j = std::move(job)]() {
                j();
                remaining.fetch_sub(1, std::memory_order_release);
            });
        }

        // Spin-wait with yield (short jobs) or help execute
        while (remaining.load(std::memory_order_acquire) > 0) {
            Job stolen;
            {
                std::lock_guard lock(m_mutex);
                if (!m_jobQueue.empty()) {
                    stolen = std::move(m_jobQueue.front());
                    m_jobQueue.pop();
                }
            }
            if (stolen) {
                stolen();
            } else {
                std::this_thread::yield();
            }
        }
    }

    // Parallel for: divides [0, count) into batches
    template <typename Func>
    void ParallelFor(uint32_t count, uint32_t batchSize, Func fn) {
        if (count == 0) return;
        if (batchSize == 0) batchSize = 1;

        std::vector<Job> jobs;
        for (uint32_t start = 0; start < count; start += batchSize) {
            uint32_t end = (std::min)(start + batchSize, count);
            jobs.push_back([start, end, &fn]() {
                for (uint32_t i = start; i < end; ++i) {
                    fn(i);
                }
            });
        }
        SubmitAndWait(jobs);
    }

    uint32_t GetWorkerCount() const {
        return static_cast<uint32_t>(m_workers.size());
    }

private:
    JobSystem() = default;
    ~JobSystem() { Shutdown(); }
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    void WorkerThread() {
        while (m_running.load(std::memory_order_relaxed)) {
            Job job;
            {
                std::unique_lock lock(m_mutex);
                m_condition.wait(lock, [this] {
                    return !m_jobQueue.empty() || !m_running.load(std::memory_order_relaxed);
                });
                if (!m_running.load(std::memory_order_relaxed) && m_jobQueue.empty()) return;
                if (m_jobQueue.empty()) continue;
                job = std::move(m_jobQueue.front());
                m_jobQueue.pop();
            }
            job();
        }
    }

    std::vector<std::jthread> m_workers;
    std::queue<Job> m_jobQueue;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_running{false};
};

} // namespace ECS
