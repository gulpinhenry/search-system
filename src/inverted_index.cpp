#include "inverted_index.h"
#include "compression.h"
#include <fstream>
#include <iostream>
#include <algorithm>

InvertedIndex::InvertedIndex(const std::string &indexFilename, const std::string &lexiconFilename)
    : currentPostingIndex(0), cacheSizeLimit(1000000), currentCacheSize(0) {
    // Open index ifle
    indexFile.open(indexFilename, std::ios::binary);
    if (!indexFile.is_open()) {
        std::cerr << "Error opening index file: " << indexFilename << std::endl;
    }
    loadLexicon(lexiconFilename);
}

// load lexicon into main memory
void InvertedIndex::loadLexicon(const std::string &lexiconFilename) {
    std::ifstream lexiconFile(lexiconFilename, std::ios::binary);
    if (!lexiconFile.is_open()) {
        std::cerr << "Error opening lexicon file: " << lexiconFilename << std::endl;
        return;
    }

    while (lexiconFile.peek() != EOF) {
        // Read term length
        uint16_t termLength;
        lexiconFile.read(reinterpret_cast<char*>(&termLength), sizeof(termLength));

        // Read term
        std::string term(termLength, ' ');
        lexiconFile.read(&term[0], termLength);

        // Read entry data
        LexiconEntry entry;
        lexiconFile.read(reinterpret_cast<char*>(&entry.offset), sizeof(entry.offset));
        lexiconFile.read(reinterpret_cast<char*>(&entry.length), sizeof(entry.length));
        lexiconFile.read(reinterpret_cast<char*>(&entry.docFrequency), sizeof(entry.docFrequency));

        lexicon[term] = entry;
    }

    lexiconFile.close();
}

// Open an inverted list for a given term
bool InvertedIndex::openList(const std::string &term) {
    closeList();  // Close any previously open list

    auto it = lexicon.find(term);
    if (it == lexicon.end()) {
        // Term not found in lexicon
        return false;
    }

    currentTerm = term;
    currentPostingIndex = 0;

    // Check if the term is in the cache
    if (cache.find(term) != cache.end()) {
        currentPostings = cache[term];
        return true;
    }

    // Load postings from index file
    LexiconEntry entry = it->second;
    indexFile.seekg(entry.offset, std::ios::beg);

    std::vector<unsigned char> compressedData(entry.length);
    indexFile.read(reinterpret_cast<char*>(compressedData.data()), entry.length);

    // Decompress the postings
    std::vector<int> gaps = varbyteDecodeList(compressedData);
    std::vector<int> docIDs(gaps.size());
    docIDs[0] = gaps[0];
    for (size_t i = 1; i < gaps.size(); ++i) {
        docIDs[i] = docIDs[i - 1] + gaps[i];
    }

    // Frequencies are assumed to be 1 since we didn't store them; adjust as needed
    currentPostings.resize(docIDs.size());
    for (size_t i = 0; i < docIDs.size(); ++i) {
        currentPostings[i] = { docIDs[i], 1 };  // Frequency is 1
    }

    // Load into cache if cache size allows
    size_t postingsSize = currentPostings.size();
    if (currentCacheSize + postingsSize <= cacheSizeLimit) {
        cache[term] = currentPostings;
        currentCacheSize += postingsSize;
    }

    return true;
}

// Close the current inverted list
void InvertedIndex::closeList() {
    currentTerm.clear();
    currentPostings.clear();
    currentPostingIndex = 0;
}

// Get the next posting in the list
bool InvertedIndex::nextPosting(Posting &posting) {
    if (currentPostingIndex >= currentPostings.size()) {
        return false;
    }

    posting = currentPostings[currentPostingIndex++];
    return true;
}

// Seek to a specific docID in the list (forward seeking)
bool InvertedIndex::seek(int targetDocID, Posting &posting) {
    while (currentPostingIndex < currentPostings.size() && currentPostings[currentPostingIndex].docID < targetDocID) {
        currentPostingIndex++;
    }

    if (currentPostingIndex < currentPostings.size()) {
        posting = currentPostings[currentPostingIndex++];
        return true;
    }

    return false;
}

// Get the document frequency of the current term
int InvertedIndex::getDocFrequency(const std::string &term) const {
    auto it = lexicon.find(term);
    if (it != lexicon.end()) {
        return it->second.docFrequency;
    }
    return 0;
}

// Check if the list is open
bool InvertedIndex::isListOpen() const {
    return !currentTerm.empty();
}
