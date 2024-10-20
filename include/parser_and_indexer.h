#ifndef PARSER_AND_INDEXER_H
#define PARSER_AND_INDEXER_H

#include "utils.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>


// Function prototypes
std::vector<std::string> tokenize(const std::string &text);
void processPassage(int docID, const std::string &passage, std::vector<TermDocPair> &termDocPairs);
void generateTermDocPairs(const std::string &inputFile, std::unordered_map<int, std::string> &pageTable);
void saveTermDocPairsToFile(const std::vector<TermDocPair> &termDocPairs,const int &fileCounter);
void writePageTableToFile(const std::unordered_map<int, std::string> &pageTable);

#endif  // PARSER_AND_INDEXER_H
