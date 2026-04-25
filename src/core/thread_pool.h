#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <cassert>

// ========== ConcurrentQueue ==========
// 线程安全的 FIFO 队列，用于生产者-消费者模式。
// 支持阻塞等待和非阻塞尝试获取。
template <typename T>
class ConcurrentQueue {
public:
    void push(T item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(item));
        }
        cv_.notify_one();
    }

    // 批量推入，减少锁竞争
    void pushBatch(std::vector<T>& items) {
        if (items.empty()) return;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& item : items) {
                queue_.push(std::move(item));
            }
        }
        cv_.notify_all();
    }

    // 非阻塞尝试获取。成功返回 true，队列空返回 false。
    bool tryPop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // 批量获取最多 maxCount 个元素，减少锁竞争
    int tryPopBatch(std::vector<T>& out, int maxCount) {
        std::lock_guard<std::mutex> lock(mutex_);
        int count = 0;
        while (!queue_.empty() && count < maxCount) {
            out.push_back(std::move(queue_.front()));
            queue_.pop();
            ++count;
        }
        return count;
    }

    // 阻塞等待直到有元素可用或被通知停止
    bool waitPop(T& item, const std::atomic<bool>& stopFlag) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&] { return !queue_.empty() || stopFlag.load(std::memory_order_relaxed); });
        if (queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    // 唤醒所有等待的线程（用于 shutdown）
    void notifyAll() {
        cv_.notify_all();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
};

// ========== ThreadPool ==========
// 通用工作线程池。支持提交任意 callable 任务。
// 用于区块生成、mesh 构建等 CPU 密集型工作的并行化。
class ThreadPool {
public:
    ThreadPool() = default;
    ~ThreadPool() { shutdown(); }

    // 禁止拷贝/移动
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 初始化线程池。numThreads=0 表示自动检测（物理核心数-1，至少2）
    void init(int numThreads = 0) {
        if (running_) return;

        if (numThreads <= 0) {
            numThreads = static_cast<int>(std::thread::hardware_concurrency());
            numThreads = std::max(2, numThreads - 1); // 留一个核给主线程
        }

        running_ = true;
        stop_.store(false, std::memory_order_relaxed);

        workers_.reserve(numThreads);
        for (int i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }

    void shutdown() {
        if (!running_) return;
        running_ = false;
        stop_.store(true, std::memory_order_relaxed);
        taskQueue_.notifyAll();
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
        workers_.clear();
    }

    bool isRunning() const { return running_; }
    int threadCount() const { return static_cast<int>(workers_.size()); }

    // 提交一个任务，返回 future 用于获取结果
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using ReturnType = decltype(f(args...));

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> future = task->get_future();

        taskQueue_.push([task]() { (*task)(); });

        return future;
    }

    // 提交一个无返回值的轻量任务（避免 packaged_task 开销）
    void submitTask(std::function<void()> task) {
        taskQueue_.push(std::move(task));
    }

    // 当前待处理任务数
    size_t pendingTasks() const { return taskQueue_.size(); }

private:
    void workerLoop() {
        std::function<void()> task;
        while (taskQueue_.waitPop(task, stop_)) {
            task();
        }
    }

    std::vector<std::thread> workers_;
    ConcurrentQueue<std::function<void()>> taskQueue_;
    std::atomic<bool> stop_{false};
    bool running_ = false;
};
