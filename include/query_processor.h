#ifndef QUERY_PROCESSOR_H
#define QUERY_PROCESSOR_H

#include "inverted_index.h"
#include <string>
#include <vector>
#include <unordered_map>

class QueryProcessor {
public:
    QueryProcessor(const std::string &indexFilename, const std::string &lexiconFilename, const std::string &pageTableFilename);

    // Process a query and return the top 10 results
    void processQuery(const std::string &query, bool conjunctive);

private:
    // Helper function to parse the query into terms
    std::vector<std::string> parseQuery(const std::string &query);

    // BM25 ranking function
    double computeBM25(int docID, const std::vector<std::string> &terms, const std::unordered_map<std::string, int> &termFrequencies);

    // Load page table
    void loadPageTable(const std::string &pageTableFilename);

    // Data members
    InvertedIndex invertedIndex;
    std::unordered_map<int, std::string> pageTable;  // Maps docID to document name

    // Statistics for BM25
    int totalDocs;
    double avgDocLength;
    std::unordered_map<int, int> docLengths;  // Stores document lengths
};

#endif  // QUERY_PROCESSOR_H
