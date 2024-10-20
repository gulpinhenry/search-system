// thread_pool.h
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <vector>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t threads = 8, size_t maxThreadsInQueue = 32);
    ~ThreadPool();

    void enqueue(std::function<void()> f);
    void waitAll();

private:
    void worker();

    // Thread pool state
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    // Synchronization primitives
    std::mutex queueMutex;
    std::condition_variable taskAvailable;
    std::condition_variable spaceAvailable;
    std::condition_variable allTasksDone;  // Condition variable to signal when all tasks are done

    // Control variables
    bool stop;
    size_t maxThreadsInQueue;
    std::atomic<size_t> tasksRemaining;  // Counter for tasks remaining to be processed
};

#endif // THREADPOOL_H
