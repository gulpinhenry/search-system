#include "inverted_index.h"
#include "compression.h"
#include <iostream>
#include <cmath>  
#include <cstring> 

// --- InvertedListPointer Implementation ---

InvertedListPointer::InvertedListPointer(std::ifstream *indexFile, const LexiconEntry &lexEntry)
    : indexFile(indexFile), lexEntry(lexEntry), currentDocID(-1), valid(true),
      lastDocID(0), bufferPos(0), currentBlockIndex(0), atBlockStart(true), termFreqScoreIndex(0) {
    // Initialize by loading the first block
    loadBlock(currentBlockIndex);
}

void InvertedListPointer::loadBlock(int blockIndex) {
    if (blockIndex >= lexEntry.blockCount) {
        valid = false;
        return;
    }

    currentBlockIndex = blockIndex;
    bufferPos = 0;
    compressedData.clear();
    termFreqScores.clear();

    // Get the number of postings in this block
    int postingsInBlock = lexEntry.blockDocCounts[blockIndex];
    size_t compressedDocIDsSize = lexEntry.blockCompressedDocIDLengths[blockIndex];

    // Read compressed docIDs
    size_t blockOffset = lexEntry.blockOffsets[blockIndex];
    indexFile->seekg(blockOffset, std::ios::beg);

    compressedData.resize(compressedDocIDsSize);
    indexFile->read(reinterpret_cast<char*>(compressedData.data()), compressedDocIDsSize);

    // Read term frequency scores
    termFreqScores.resize(postingsInBlock);
    indexFile->read(reinterpret_cast<char*>(termFreqScores.data()), postingsInBlock * sizeof(float));

    // Reset bufferPos for reading docIDs
    bufferPos = 0;
    atBlockStart = true;
    lastDocID = 0;

    // Reset termFreqScoreIndex
    termFreqScoreIndex = 0;
}

bool InvertedListPointer::next() {
    if (!valid) return false;

    while (true) {
        if (termFreqScoreIndex >= termFreqScores.size()) {
            // End of current block
            currentBlockIndex++;
            if (currentBlockIndex >= lexEntry.blockCount) {
                valid = false;
                return false;
            } else {
                loadBlock(currentBlockIndex);
                continue;
            }
        }

        int docID;
        if (atBlockStart) {
            // First docID in block is stored as absolute value
            docID = varbyteDecodeNumber(compressedData, bufferPos);
            atBlockStart = false;
        } else {
            // Decompress next docID gap
            int gap = varbyteDecodeNumber(compressedData, bufferPos);
            docID = lastDocID + gap;
        }
        lastDocID = docID;
        currentDocID = docID;

        // Get term frequency score
        termFreqScoreValue = termFreqScores[termFreqScoreIndex];
        termFreqScoreIndex++;

        // Successfully read posting
        return true;
    }
}

bool InvertedListPointer::nextGEQ(int docID) {
    if (!valid) return false;

    // Skip blocks where blockMaxDocID < docID
    while (currentBlockIndex < lexEntry.blockCount && lexEntry.blockMaxDocIDs[currentBlockIndex] < docID) {
        currentBlockIndex++;
        if (currentBlockIndex >= lexEntry.blockCount) {
            valid = false;
            return false;
        }
        loadBlock(currentBlockIndex);
    }

    // Now, iterate through postings in the block until we find docID >= target docID
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
    return termFreqScoreValue;
}

float InvertedListPointer::getIDF() const {
    return lexEntry.IDF;
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

    const int64_t documentLen = 8841823; // Total number of documents; adjust as necessary

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

            // Read blockCompressedDocIDLengths
            entry.blockCompressedDocIDLengths.resize(entry.blockCount);
            for (int i = 0; i < entry.blockCount; ++i) {
                lexiconFile.read(reinterpret_cast<char*>(&entry.blockCompressedDocIDLengths[i]), sizeof(size_t));
            }

            // Read blockDocCounts
            entry.blockDocCounts.resize(entry.blockCount);
            for (int i = 0; i < entry.blockCount; ++i) {
                lexiconFile.read(reinterpret_cast<char*>(&entry.blockDocCounts[i]), sizeof(entry.blockDocCounts[i]));
            }
        }

        // Compute IDF
        entry.IDF = std::log((documentLen - entry.docFrequency + 0.5) / (entry.docFrequency + 0.5));

        lexicon[term] = entry;
    }
    lexiconFile.close();
}

bool InvertedIndex::openList(const std::string &term) {
    return lexicon.find(term) != lexicon.end();
}

InvertedListPointer InvertedIndex::getListPointer(const std::string &term) {
    auto it = lexicon.find(term);
    if (it != lexicon.end()) {
        return InvertedListPointer(&indexFile, it->second);
    } else {
        // Handle term not found
        std::cerr << "Term not found in lexicon: " << term << std::endl;
        // Return an invalid InvertedListPointer
        LexiconEntry emptyEntry;
        return InvertedListPointer(nullptr, emptyEntry);
    }
}

void InvertedIndex::closeList(const std::string &term) {
    // No action needed as we're not keeping any state per term
}

int InvertedIndex::getDocFrequency(const std::string &term) {
    auto it = lexicon.find(term);
    if (it != lexicon.end()) {
        return it->second.docFrequency;
    } else {
        return 0;
    }
}
