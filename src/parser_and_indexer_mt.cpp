#include "parser_and_indexer_mt.h"
#include "thread_pool.h"
#include "utils.h"
#include <thread>
#include <mutex>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <sys/stat.h>
#include <ctime>
#include <cstdint>
#include <atomic>
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

// Log messages to a file (for debugging purposes)
std::ofstream logFile("../logs/parserMT.log", std::ios::app);
void logMessage(const std::string &message)
{
    if (!logFile.is_open())
    {
        std::cerr << "Failed to open log file!" << std::endl;
        return;
    }
    std::time_t currentTime = std::time(nullptr);
    logFile << std::asctime(std::localtime(&currentTime)) << message << std::endl;
}
void processPassageMT(int docID, const std::string &passage, std::vector<TermDocPair> &termDocPairs, std::mutex &termDocPairsMutex, std::atomic<int> &fileCounter, std::unordered_map<int, int> &docLengths, std::mutex &docLengthsMutex)
{
    logMessage("Processing passage for docID: " + std::to_string(docID));
    auto terms = tokenize(passage);

    // Store the document length
    {
        std::lock_guard<std::mutex> docLengthsLock(docLengthsMutex);
        docLengths[docID] = terms.size();
    }

    // Reserve space to avoid frequent reallocations
    {
        std::unique_lock<std::mutex> lock(termDocPairsMutex);
        if (termDocPairs.capacity() < MAX_RECORDS) {
            termDocPairs.reserve(MAX_RECORDS);
        }
    }

    // Lock the mutex while modifying termDocPairs
    std::unique_lock<std::mutex> termDocPairsLock(termDocPairsMutex);

    // Add terms to termDocPairs
    for (const auto &term : terms) {
        termDocPairs.emplace_back(term, docID);
    }

    // If the vector reaches the max size, move its contents to a local copy
    if (termDocPairs.size() >= MAX_RECORDS) {
        std::vector<TermDocPair> termDocPairsCopy(std::move(termDocPairs));
        termDocPairs.clear();  // Clear the original vector for reuse

        // Unlock the mutex early since we're done with the shared data
        termDocPairsLock.unlock();

        // Increment the file counter atomically
        int curFileCounter = fileCounter.fetch_add(1);

        // Save the copied data to a file asynchronously
        std::cout << "Writing to file with fileCounter: " << curFileCounter << std::endl;
        saveTermDocPairsToFile(termDocPairsCopy, curFileCounter);
    }
}



// function similar to generateTermDocPairs but with multi threading
void generateTermDocPairsMT(const std::string &inputFile, std::unordered_map<int, std::string> &pageTable, ThreadPool *threadPool, std::unordered_map<int, int> &docLengths, std::mutex &docLengthsMutex)
{
    std::ifstream inputFileStream(inputFile);
    if (!inputFileStream.is_open())
    {
        logMessage("Error opening input file: " + inputFile);
        return;
    }
    std::mutex streamMutex, pageTableMutex, termDocPairsMutex;
    std::vector<TermDocPair> termDocPairs;
    std::string line;
    std::atomic<int> docID(0);
    std::atomic<int> fileCounter{0};

    while (std::getline(inputFileStream, line))
    {
        auto task = [line, &pageTableMutex, &pageTable, &docID, &termDocPairs, &termDocPairsMutex, &fileCounter, &docLengths, &docLengthsMutex]()
        {
            size_t tabPos = line.find('\t');
            if (tabPos != std::string::npos)
            {
                std::string docName = line.substr(0, tabPos);
                std::string passage = line.substr(tabPos + 1);
                int _docId = docID++;

                {
                    std::lock_guard<std::mutex> pageTableLock(pageTableMutex);
                    // Store in page table
                    pageTable[_docId] = docName;
                }

                processPassageMT(_docId, passage, termDocPairs, termDocPairsMutex, fileCounter, docLengths, docLengthsMutex);
            }
        };
        if (threadPool)
        {
            threadPool->enqueue(task);
        }
        else
        {
            logMessage("ThreadPool is not defined. Trying single thread one");
            task();
        }
    }

    // Wait for all tasks to finish
    threadPool->waitAll();

    // Clean up the thread pool
    delete threadPool;

    // Handle any remaining term-doc pairs
    auto task = [&termDocPairs, &termDocPairsMutex, &fileCounter]()
    {
        std::lock_guard<std::mutex> termDocPairsLock(termDocPairsMutex);
        if (termDocPairs.size() > 0)
        {
            std::cout << "Write remaining TermDocPairs to temp." << std::endl;
            saveTermDocPairsToFile(termDocPairs, fileCounter++);
        }
        else
            std::cout << "TermDocPairs size zero! No need to create new" << std::endl;
    };
    task();
}



#include <chrono>

int main(int argc, char *argv[])
{
    auto startTime = std::chrono::high_resolution_clock::now();
    int threadNum = 8; // Default thread number

    // Check if a command line argument is provided
    if (argc > 1)
    {
        threadNum = std::atoi(argv[1]); // Convert argument to integer
    }
    int maxWorks = 16;
    if (argc > 2)
    {
        maxWorks = std::atoi(argv[2]); // Convert argument to integer
    }

    if (threadNum <= 0)
    {
        threadNum = 8;
    }

    if (maxWorks <= 0)
    {
        maxWorks = 16;
    }
    {
        ThreadPool *pool = new ThreadPool(threadNum, maxWorks);

        createDirectory("../data");
        createDirectory("../data/intermediate");

        // Data structures for the page table and document lengths
        std::unordered_map<int, std::string> pageTable;
        std::unordered_map<int, int> docLengths;
        std::mutex docLengthsMutex;

        generateTermDocPairsMT("../data/collection_short.tsv", pageTable, pool, docLengths, docLengthsMutex);

        // Write the page table to file
        writePageTableToFile(pageTable);

        // Write the document lengths to file
        writeDocLengthsToFile(docLengths);

        logMessage("Parsing process with multi-threading completed.");
    }
    auto endTime = std::chrono::high_resolution_clock::now();

    std::cout << "time passed: " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << std::endl;

    return 0;
}
