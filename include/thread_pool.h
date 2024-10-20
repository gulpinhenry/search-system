#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(size_t threads = 8, size_t maxThreadsInQueue = 32);
    ~ThreadPool();

    void enqueue(std::function<void()> f);

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queueMutex;
    std::condition_variable taskAvailable, spaceAvailable;
    bool stop = false;

    size_t maxThreadsInQueue;

    void worker();
};

#endif // THREADPOOL_H