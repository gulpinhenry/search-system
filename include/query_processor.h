// query_processor.h
#ifndef QUERY_PROCESSOR_H
#define QUERY_PROCESSOR_H

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <fstream>

// Lexicon entry structure
struct LexiconEntry {
    int64_t offset;
    int32_t length;
    int32_t docFrequency;
    int32_t blockCount;
    std::vector<int32_t> blockMaxDocIDs; // Maximum docID in each block
    std::vector<int64_t> blockOffsets;   // Offset of each block in the index file
};

class InvertedListPointer {
public:
    InvertedListPointer(std::ifstream *indexFile, const LexiconEntry &lexEntry);
    bool next();
    bool nextGEQ(int docID);
    int getDocID() const;
    int getTF() const; 
    bool isValid() const;
    void close();
private:
    std::ifstream *indexFile;
    LexiconEntry lexEntry;
    int currentDocID;
    bool valid;
    int lastDocID;
    size_t bufferPos;
    std::vector<unsigned char> compressedData;
    std::vector<float> termFreqScore;
};

class InvertedIndex {
public:
    InvertedIndex(const std::string &indexFilename, const std::string &lexiconFilename);
    bool openList(const std::string &term);
    InvertedListPointer getListPointer(const std::string &term);
    void closeList(const std::string &term);
    int getDocFrequency(const std::string &term); // New method
private:
    std::ifstream indexFile;
    std::unordered_map<std::string, LexiconEntry> lexicon;

    void loadLexicon(const std::string &lexiconFilename);
};

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
