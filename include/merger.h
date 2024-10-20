// merger.h
#ifndef MERGER_H
#define MERGER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

// Lexicon entry structure
struct LexiconEntry {
    int64_t offset;                // Byte offset within the index file where the posting list starts
    int32_t length;                // Length of the posting list data in bytes
    int32_t docFrequency;          // Number of documents containing the term
    int32_t blockCount;            // Number of blocks for this term
    std::vector<int32_t> blockMaxDocIDs; // Maximum docID in each block
    std::vector<int64_t> blockOffsets;   // Offset of each block in the index file
};

// Function prototypes
void mergeTempFiles(int numFiles, std::unordered_map<std::string, LexiconEntry> &lexicon);
void writeLexiconToFile(const std::unordered_map<std::string, LexiconEntry> &lexicon);
void logMessage(const std::string &message);

#endif  // MERGER_H
