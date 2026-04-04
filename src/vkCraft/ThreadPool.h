#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>

/**
 * A fixed-size thread pool. Tasks are submitted via enqueue() and run in
 * parallel on background threads. Returns std::future for result/error
 * propagation. Safe to destroy while idle (joins all threads).
 */
class ThreadPool
{
public:
    explicit ThreadPool(size_t numThreads)
    {
        for (size_t i = 0; i < numThreads; i++)
        {
            workers.emplace_back([this]
            {
                for (;;)
                {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queueMutex);
                        condition.wait(lock, [this]
                        {
                            return stopped || !tasks.empty();
                        });
                        if (stopped && tasks.empty())
                            return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stopped = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers)
            worker.join();
    }

    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * Enqueue a callable and return a future for its result.
     * The callable is executed on a background worker thread.
     */
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>
    {
        using ReturnType = typename std::result_of<F(Args...)>::type;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (stopped)
                throw std::runtime_error("ThreadPool: enqueue on stopped pool");
            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return result;
    }

    size_t threadCount() const { return workers.size(); }

private:
    std::vector<std::thread>          workers;
    std::queue<std::function<void()>> tasks;
    std::mutex                        queueMutex;
    std::condition_variable           condition;
    bool                              stopped = false;
};

#endif // THREAD_POOL_H
