#ifndef INVERTED_INDEX
#define INVERTED_INDEX
#include <fstream>
#include "lexicon_entry.h"
#include <unordered_map>

class InvertedListPointer {
public:
    InvertedListPointer(std::ifstream *indexFile, const LexiconEntry &lexEntry);
    bool next();
    bool nextGEQ(int docID);
    int getDocID() const;
    float getTFS() const;
    int getTF() const; 
    bool isValid() const;
    void close();
    float getIDF() const;
private:
    std::ifstream *indexFile;
    LexiconEntry lexEntry;
    int currentDocID;
    bool valid;
    int lastDocID;
    size_t bufferPos;
    size_t termFreqScoreIndex;
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
#endif