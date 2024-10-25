#ifndef INVERTED_INDEX_H
#define INVERTED_INDEX_H

#include <fstream>
#include <unordered_map>
#include <string>
#include <vector>
#include "lexicon_entry.h"

class InvertedListPointer {
public:
    InvertedListPointer(std::ifstream *indexFile, const LexiconEntry &lexEntry);
    bool next();
    bool nextGEQ(int docID);
    int getDocID() const;
    float getTFS() const;
    bool isValid() const;
    void close();
    float getIDF() const;

private:
    void loadBlock(int blockIndex);

    std::ifstream *indexFile;
    LexiconEntry lexEntry;
    int currentDocID;
    bool valid;
    int lastDocID;
    size_t bufferPos;
    std::vector<unsigned char> compressedData;
    std::vector<float> termFreqScores;  // Added to store term frequency scores separately
    float termFreqScoreValue;
    int currentBlockIndex;
    bool atBlockStart;
    size_t termFreqScoreIndex;  // Added to track position in termFreqScores
};

class InvertedIndex {
public:
    InvertedIndex(const std::string &indexFilename, const std::string &lexiconFilename);
    bool openList(const std::string &term);
    InvertedListPointer getListPointer(const std::string &term);
    void closeList(const std::string &term);
    int getDocFrequency(const std::string &term);

private:
    std::ifstream indexFile;
    std::unordered_map<std::string, LexiconEntry> lexicon;

    void loadLexicon(const std::string &lexiconFilename);
};

#endif // INVERTED_INDEX_H
