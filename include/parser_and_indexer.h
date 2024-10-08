#ifndef PARSER_AND_INDEXER_H
#define PARSER_AND_INDEXER_H

#include <string>
#include <vector>
#include <unordered_map>

struct Posting {
    int docID;
    int frequency;
};

// Function prototypes
std::vector<std::string> tokenize(const std::string &text);
void processPassage(int docID, const std::string &passage, std::unordered_map<std::string, std::vector<Posting>> &index);
void generateCompressedIndex(const std::string &inputFile);
void writeCompressedBlock(std::ofstream &binFile, const std::vector<int> &docIDs, const std::vector<int> &frequencies,
                          std::vector<int> &lastdocid, std::vector<int> &docidsize, std::vector<int> &freqsize);
void writeMetadata(const std::vector<int> &lastdocid, const std::vector<int> &docidsize, const std::vector<int> &freqsize);

#endif
