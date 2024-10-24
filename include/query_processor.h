// query_processor.h
#ifndef QUERY_PROCESSOR_H
#define QUERY_PROCESSOR_H
#include "inverted_index.h"
#include "lexicon_entry.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <fstream>

class QueryProcessor {
public:
    QueryProcessor(const std::string &indexFilename, const std::string &lexiconFilename, const std::string &pageTableFilename, const std::string &docLengthsFilename);
    void processQuery(const std::string &query, bool conjunctive);
private:
    InvertedIndex invertedIndex;
    std::unordered_map<int, std::string> pageTable; // docID -> docName
    std::unordered_map<int, int> docLengths;        // docID -> docLength
    int totalDocs;
    double avgDocLength;

    std::vector<std::string> parseQuery(const std::string &query);
    void loadPageTable(const std::string &pageTableFilename);
    void loadDocumentLengths(const std::string &docLengthsFilename);
};

#endif // QUERY_PROCESSOR_H
