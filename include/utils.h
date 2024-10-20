#ifndef UTILS_H
#define UTILS_H
#include <string>
#include <unordered_map>
#include <vector>

#define MAX_RECORDS 1000000  // Maximum number of term-docID pairs in memory


// Term-Document Pair structure to store
struct TermDocPair {
    std::string term;
    int docID;

    // Constructor
    TermDocPair(const std::string& t, int d) : term(t), docID(d) {}

    // Default constructor (needed by std::vector)
    TermDocPair() = default;

    // Copy constructor
    TermDocPair(const TermDocPair& other) = default;

    // Copy assignment operator
    TermDocPair& operator=(const TermDocPair& other) = default;

    // Move constructor
    TermDocPair(TermDocPair&& other) noexcept = default;

    // Move assignment operator
    TermDocPair& operator=(TermDocPair&& other) noexcept = default;
};


// Helper function to create directories for data 
void createDirectory(const std::string &dir);
std::vector<std::string> tokenize(const std::string &text);
void saveTermDocPairsToFile(const std::vector<TermDocPair> &termDocPairs, const int &fileCounter);
void writePageTableToFile(const std::unordered_map<int, std::string> &pageTable);
void writeDocLengthsToFile(const std::unordered_map<int, int> &docLengths);


#endif