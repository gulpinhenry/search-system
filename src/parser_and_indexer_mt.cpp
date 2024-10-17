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
void processPassageMT(int docID, const std::string &passage, std::vector<TermDocPair> &termDocPairs, std::mutex &termDocPairsMutex, int &fileCounter)
{
    logMessage("Processing passage for docID: " + std::to_string(docID));
    auto terms = tokenize(passage);

    std::lock_guard<std::mutex> termDocPairsLock(termDocPairsMutex);
    // For each term, generate a TermDocPair
    for (const auto &term : terms)
    {
        termDocPairs.push_back({term, docID});
    }
    if (termDocPairs.size() >= MAX_RECORDS)
    {
        saveTermDocPairsToFile(termDocPairs, fileCounter++);
        termDocPairs.clear(); // Clear buffer after saving
    }
}

// function similar to generateTermDocPairs but with multi threading
void generateTermDocPairsMT(const std::string &inputFile, std::unordered_map<int, std::string> &pageTable, ThreadPool *threadPool)
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
    int fileCounter = 0;

    while (std::getline(inputFileStream, line))
    {
        if (threadPool)
        {
            auto task = [line, &pageTableMutex, &pageTable, &docID, &termDocPairs, &termDocPairsMutex, &fileCounter]()
            {
                size_t tabPos = line.find('\t');
                if (tabPos != std::string::npos)
                {
                    std::string docName = line.substr(0, tabPos);
                    std::string passage = line.substr(tabPos + 1);

                    {
                        std::lock_guard<std::mutex> pageTableLock(pageTableMutex);
                        // Store in page table
                        pageTable[docID] = docName;
                    }

                    processPassageMT(docID++, passage, termDocPairs, termDocPairsMutex, fileCounter);
                } };
            threadPool->enqueue(task);
        }
        else
        {
            logMessage("ThreadPool is not defined. Try single thread one?");
            exit(-1);
        }
    }
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

    if (threadNum <= 0)
    {
        threadNum = 8;
    }
    ThreadPool pool(threadNum);

    createDirectory("../data");
    createDirectory("../data/intermediate");

    // Data structures for the page table
    std::unordered_map<int, std::string> pageTable;

    generateTermDocPairsMT("../data/collection.tsv", pageTable, &pool);

    // Write the page table to file
    writePageTableToFile(pageTable);

    logMessage("Parsing process with multi-threading completed.");
    auto endTime = std::chrono::high_resolution_clock::now();

    std::cout << "time passed: " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << std::endl;

    return 0;
}