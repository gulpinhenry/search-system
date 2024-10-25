#ifndef LEXICON_ENTRY_H
#define LEXICON_ENTRY_H

#include <vector>
#include <cstdint>

// Lexicon entry structure
struct LexiconEntry {
    int64_t offset;
    int32_t length;
    int32_t docFrequency;
    int32_t blockCount;
    std::vector<int32_t> blockMaxDocIDs;          // Maximum docID in each block
    std::vector<int64_t> blockOffsets;            // Offset of each block in the index file
    std::vector<size_t> blockCompressedDocIDLengths; // Length of compressed docIDs in each block
    std::vector<int32_t> blockDocCounts;          // Number of postings in each block
    float IDF;

    // Method to get block length
    size_t getBlockLength(int blockIndex) const {
        if (blockIndex < blockCount - 1) {
            return blockOffsets[blockIndex + 1] - blockOffsets[blockIndex];
        } else {
            return (offset + length) - blockOffsets[blockIndex];
        }
    }
};

#endif // LEXICON_ENTRY_H
