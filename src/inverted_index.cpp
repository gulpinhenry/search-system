#include "inverted_index.h"
#include "compression.h"
#include <iostream>
#include <math.h>

// Constants for BM25
const double k1 = 1.5;
const double b = 0.75;
const int64_t documentLen = 8841823;

// --- InvertedListPointer Implementation ---

InvertedListPointer::InvertedListPointer(std::ifstream *indexFile, const LexiconEntry &lexEntry)
    : indexFile(indexFile), lexEntry(lexEntry), currentDocID(-1), valid(true),
      lastDocID(0), bufferPos(0), termFreqScoreIndex(-1) {
    // Read compressed data from index file
    indexFile->seekg(lexEntry.offset, std::ios::beg);
    compressedData.resize(lexEntry.length);
    indexFile->read(reinterpret_cast<char*>(compressedData.data()), lexEntry.length);
    termFreqScore.resize(lexEntry.length);
    indexFile->read(reinterpret_cast<char*>(termFreqScore.data()), lexEntry.length * sizeof(float));
}

bool InvertedListPointer::next() {
    if (!valid) return false;

    if (bufferPos >= compressedData.size()) {
        valid = false;
        return false;
    }
    termFreqScoreIndex++;
    if (termFreqScoreIndex >= termFreqScore.size()) {
        valid = false;
        return false;
    }

    // Decompress next docID gap
    int gap = varbyteDecodeNumber(compressedData, bufferPos);
    lastDocID += gap;
    currentDocID = lastDocID;

    return true;
}

bool InvertedListPointer::nextGEQ(int docID) {
    while (valid && currentDocID < docID) {
        if (!next()) {
            return false;
        }
    }
    return valid;
}

int InvertedListPointer::getDocID() const {
    return currentDocID;
}

float InvertedListPointer::getTFS() const {
    return termFreqScore[termFreqScoreIndex];
}

float InvertedListPointer::getIDF() const {
    return lexEntry.IDF;
}

int InvertedListPointer::getTF() const {
    // Since TF is not stored, we might return a default value
    return 1;
}

int InvertedIndex::getDocFrequency(const std::string &term) {
    auto it = lexicon.find(term);
    if (it != lexicon.end()) {
        return it->second.docFrequency;
    } else {
        return 0;
    }
}


bool InvertedListPointer::isValid() const {
    return valid;
}

void InvertedListPointer::close() {
    valid = false;
}

// --- InvertedIndex Implementation ---

InvertedIndex::InvertedIndex(const std::string &indexFilename, const std::string &lexiconFilename) {
    // Load the lexicon from lexiconFilename
    loadLexicon(lexiconFilename);

    // Open index file
    indexFile.open(indexFilename, std::ios::binary);
    if (!indexFile.is_open()) {
        std::cerr << "Error opening index file: " << indexFilename << std::endl;
        return;
    }
}

void InvertedIndex::loadLexicon(const std::string &lexiconFilename) {
    std::ifstream lexiconFile(lexiconFilename, std::ios::binary);
    if (!lexiconFile.is_open()) {
        std::cerr << "Error opening lexicon file: " << lexiconFilename << std::endl;
        return;
    }

    while (lexiconFile.peek() != EOF) {
        uint16_t termLength;
        lexiconFile.read(reinterpret_cast<char*>(&termLength), sizeof(termLength));
        if (!lexiconFile) break; // EOF or error

        std::vector<char> termBuffer(termLength);
        lexiconFile.read(termBuffer.data(), termLength);
        if (!lexiconFile) break; // EOF or error

        std::string term(termBuffer.begin(), termBuffer.end());

        LexiconEntry entry;
        lexiconFile.read(reinterpret_cast<char*>(&entry.offset), sizeof(entry.offset));
        lexiconFile.read(reinterpret_cast<char*>(&entry.length), sizeof(entry.length));
        lexiconFile.read(reinterpret_cast<char*>(&entry.docFrequency), sizeof(entry.docFrequency));
        lexiconFile.read(reinterpret_cast<char*>(&entry.blockCount), sizeof(entry.blockCount));

        entry.IDF = log((documentLen - entry.docFrequency + 0.5) / (entry.docFrequency + 0.5));

        // If using blocking, read block metadata
        if (entry.blockCount > 0) {
            // Read blockMaxDocIDs
            entry.blockMaxDocIDs.resize(entry.blockCount);
            for (int i = 0; i < entry.blockCount; ++i) {
                lexiconFile.read(reinterpret_cast<char*>(&entry.blockMaxDocIDs[i]), sizeof(entry.blockMaxDocIDs[i]));
            }

            // Read blockOffsets
            entry.blockOffsets.resize(entry.blockCount);
            for (int i = 0; i < entry.blockCount; ++i) {
                lexiconFile.read(reinterpret_cast<char*>(&entry.blockOffsets[i]), sizeof(entry.blockOffsets[i]));
            }
        }

        lexicon[term] = entry;
    }
    lexiconFile.close();
}

bool InvertedIndex::openList(const std::string &term) {
    return lexicon.find(term) != lexicon.end();
}

InvertedListPointer InvertedIndex::getListPointer(const std::string &term) {
    return InvertedListPointer(&indexFile, lexicon[term]);
}

void InvertedIndex::closeList(const std::string &term) {
    // No action needed as we're not keeping any state per term
}