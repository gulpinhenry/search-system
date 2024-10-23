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
    std::vector<int32_t> blockMaxDocIDs; // Maximum docID in each block
    std::vector<int64_t> blockOffsets;   // Offset of each block in the index file
    float IDF;
};

#endif // LEXICON_ENTRY_H