// thread_pool.cpp
#include "thread_pool.h"
#include <iostream>

ThreadPool::ThreadPool(size_t threads, size_t maxThreadsInQueue)
    : stop(false), maxThreadsInQueue(maxThreadsInQueue), tasksRemaining(0)
{
    for (size_t i = 0; i < threads; ++i)
    {
        workers.emplace_back([this]
                             { worker(); });
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    taskAvailable.notify_all();
    for (std::thread &worker : workers)
    {
        worker.join();
    }
}

void ThreadPool::enqueue(std::function<void()> f)
{
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        // Wait if the queue is full
        while (tasks.size() >= maxThreadsInQueue) {
            spaceAvailable.wait(lock);
        }
        tasks.emplace(std::move(f));
        ++tasksRemaining;  // Increment tasks remaining
    }
    taskAvailable.notify_one();
}

void ThreadPool::worker()
{
    while (true)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            taskAvailable.wait(lock, [this]
                               { return stop || !tasks.empty(); });
            if (stop && tasks.empty())
                return;

            task = std::move(tasks.front());
            tasks.pop();
            spaceAvailable.notify_one();
        }
        // Execute the task outside the lock
        task();

        // Decrement tasks remaining and notify if zero
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (--tasksRemaining == 0) {
                allTasksDone.notify_all();
            }
        }
    }
}

void ThreadPool::waitAll()
{
    std::unique_lock<std::mutex> lock(queueMutex);
    allTasksDone.wait(lock, [this]
                      { return tasksRemaining == 0; });
}
