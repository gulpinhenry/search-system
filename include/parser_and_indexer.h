#ifndef PARSER_AND_INDEXER_H
#define PARSER_AND_INDEXER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

// Term-Document Pair structure to store
struct TermDocPair {
    std::string term;
    int docID;
};

// Function prototypes
void createDirectory(const std::string &dir);
std::vector<std::string> tokenize(const std::string &text);
void processPassage(int docID, const std::string &passage, std::vector<TermDocPair> &termDocPairs);
void generateTermDocPairs(const std::string &inputFile, std::unordered_map<int, std::string> &pageTable);
void saveTermDocPairsToFile(const std::vector<TermDocPair> &termDocPairs, int &fileCounter);
void writePageTableToFile(const std::unordered_map<int, std::string> &pageTable);

#endif  // PARSER_AND_INDEXER_H
