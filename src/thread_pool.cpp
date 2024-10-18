#include "thread_pool.h"

ThreadPool::ThreadPool(size_t threads, size_t maxThreadsInQueue) : stop(false), maxThreadsInQueue(maxThreadsInQueue)
{
    for (size_t i = 0; i < threads; ++i)
    {
        workers.emplace_back([this]
                             { worker(); });
    }
}
#include <iostream>

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
        std::cout << "THREAD END" << std::endl;
    }
}

void ThreadPool::enqueue(std::function<void()> f)
{
    {
        std::unique_lock<std::mutex> lock(queueMutex);
         // Check if the queue is full before adding new tasks
        while (tasks.size() >= maxThreadsInQueue) {
            spaceAvailable.wait(lock); // Wait until there's space in the queue
        }
        tasks.emplace(std::move(f));
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
        task();
    }
}