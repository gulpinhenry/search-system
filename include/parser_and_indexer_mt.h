#ifndef PARSER_AND_INDEXER_MT_H
#define PARSER_AND_INDEXER_MT_H


#include <string>
#include <unordered_map>

class ThreadPool;


void generateTermDocPairsMT(const std::string &inputFile, std::unordered_map<int, std::string> &pageTable, ThreadPool *threadPool);

#endif  // PARSER_AND_INDEXER_H