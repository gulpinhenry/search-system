#ifndef MERGER_H
#define MERGER_H

#include <string>
#include <unordered_map>
#include <cstdint>

// Lexicon entry to store metadata
struct LexiconEntry {
    int64_t offset;      // Byte offset within the index file where the posting list starts
    int length;          // Length of the posting list data in bytes
    int docFrequency;    // Number of documents containing the term
};

// Function prototypes
void mergeTempFiles(int numFiles, std::unordered_map<std::string, LexiconEntry> &lexicon);
void writeLexiconToFile(const std::unordered_map<std::string, LexiconEntry> &lexicon);
void logMessage(const std::string &message);

#endif  // MERGER_H

