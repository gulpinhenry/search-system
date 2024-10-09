#ifndef INVERTED_INDEX_H
#define INVERTED_INDEX_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <fstream>

struct LexiconEntry {
    int64_t offset;      // Byte offset within the index file where the posting list starts
    int length;          // Length of the posting list data in bytes
    int docFrequency;    // Number of documents containing the term
};

// posting in inverted list
struct Posting {
    int docID;
    int frequency;
};

// inverted index api
class InvertedIndex {
public:
    InvertedIndex(const std::string &indexFilename, const std::string &lexiconFilename);

    // Open an inverted list for a given term
    bool openList(const std::string &term);

    // Close the current inverted list
    void closeList();

    // Get the next posting in the list
    bool nextPosting(Posting &posting);

    // Seek to a specific docID in the list (forward seeking)
    bool seek(int targetDocID, Posting &posting);

    // Get the document frequency of the current term
    int getDocFrequency(const std::string &term) const;

    // Check if the list is open
    bool isListOpen() const;

private:
    void decompressNextBlock();

    // Cache management
    void loadListIntoCache(const std::string &term);

    // Data members
    std::ifstream indexFile;
    std::unordered_map<std::string, LexiconEntry> lexicon;
    std::string currentTerm;
    size_t currentPostingIndex;
    std::vector<Posting> currentPostings;

    // Cache for inverted lists
    std::unordered_map<std::string, std::vector<Posting>> cache;
    size_t cacheSizeLimit;  // Maximum number of postings in the cache
    size_t currentCacheSize;

    // Lexicon functions
    void loadLexicon(const std::string &lexiconFilename);
};

#endif  // INVERTED_INDEX_H
