#ifndef PARSER_AND_INDEXER_MT_H
#define PARSER_AND_INDEXER_MT_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <atomic>
#include "utils.h"

class ThreadPool;

void generateTermDocPairsMT(const std::string &inputFile, std::unordered_map<int, std::string> &pageTable, ThreadPool *threadPool, std::unordered_map<int, int> &docLengths, std::mutex &docLengthsMutex);

void processPassageMT(int docID, const std::string &passage, std::vector<TermDocPair> &termDocPairs, std::mutex &termDocPairsMutex, std::atomic<int> &fileCounter, std::unordered_map<int, int> &docLengths, std::mutex &docLengthsMutex);

#endif  // PARSER_AND_INDEXER_MT_H
