#include "compression.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

// Updated LexiconEntry struct to include docIDsLength
struct LexiconEntry {
    int64_t offset;
    int32_t length;          // Total length of postings list in bytes
    int32_t docFrequency;    // Number of documents
    int32_t docIDsLength;    // Length of compressed docIDs in bytes
};

// Function to read and print the lexicon file
void readAndPrintLexicon(const std::string &filename) {
    std::ifstream lexiconFile(filename, std::ios::binary);
    if (!lexiconFile.is_open()) {
        std::cerr << "Error opening lexicon file: " << filename << std::endl;
        return;
    }

    std::cout << "\nLexicon Contents:\n";
    while (lexiconFile.peek() != EOF) {
        // Read term length
        uint16_t termLength;
        lexiconFile.read(reinterpret_cast<char*>(&termLength), sizeof(termLength));

        // Read term
        std::string term(termLength, ' ');
        lexiconFile.read(&term[0], termLength);

        // Read LexiconEntry data
        LexiconEntry entry;
        lexiconFile.read(reinterpret_cast<char*>(&entry.offset), sizeof(entry.offset));
        lexiconFile.read(reinterpret_cast<char*>(&entry.length), sizeof(entry.length));
        lexiconFile.read(reinterpret_cast<char*>(&entry.docFrequency), sizeof(entry.docFrequency));
        lexiconFile.read(reinterpret_cast<char*>(&entry.docIDsLength), sizeof(entry.docIDsLength));

        // Print the lexicon entry
        std::cout << "Term: " << term << ", Offset: " << entry.offset
                  << ", Length: " << entry.length << ", DocFrequency: " << entry.docFrequency
                  << ", DocIDsLength: " << entry.docIDsLength << std::endl;
    }

    lexiconFile.close();
}

// Function to read and print the inverted index
void readAndPrintInvertedIndex(const std::string &indexFilename, const std::string &lexiconFilename) {
    std::ifstream indexFile(indexFilename, std::ios::binary);
    if (!indexFile.is_open()) {
        std::cerr << "Error opening index file: " << indexFilename << std::endl;
        return;
    }

    std::ifstream lexiconFile(lexiconFilename, std::ios::binary);
    if (!lexiconFile.is_open()) {
        std::cerr << "Error opening lexicon file: " << lexiconFilename << std::endl;
        return;
    }

    std::cout << "\nInverted Index Contents:\n";
    while (lexiconFile.peek() != EOF) {
        // Read term length
        uint16_t termLength;
        lexiconFile.read(reinterpret_cast<char*>(&termLength), sizeof(termLength));

        // Read term
        std::string term(termLength, ' ');
        lexiconFile.read(&term[0], termLength);

        // Read LexiconEntry data
        LexiconEntry entry;
        lexiconFile.read(reinterpret_cast<char*>(&entry.offset), sizeof(entry.offset));
        lexiconFile.read(reinterpret_cast<char*>(&entry.length), sizeof(entry.length));
        lexiconFile.read(reinterpret_cast<char*>(&entry.docFrequency), sizeof(entry.docFrequency));
        lexiconFile.read(reinterpret_cast<char*>(&entry.docIDsLength), sizeof(entry.docIDsLength));

        // Read postings list from index file
        indexFile.seekg(entry.offset, std::ios::beg);
        std::vector<unsigned char> compressedData(entry.length);
        indexFile.read(reinterpret_cast<char*>(compressedData.data()), entry.length);

        // Split compressed data into docIDs and BM25 scores
        std::vector<unsigned char> compressedDocIDs(
            compressedData.begin(),
            compressedData.begin() + entry.docIDsLength
        );
        std::vector<unsigned char> compressedBM25Scores(
            compressedData.begin() + entry.docIDsLength,
            compressedData.end()
        );

        // Decode docIDs
        std::vector<int> gaps = varbyteDecodeList(compressedDocIDs);
        std::vector<int> docIDs(gaps.size());
        docIDs[0] = gaps[0];
        for (size_t i = 1; i < gaps.size(); ++i) {
            docIDs[i] = docIDs[i - 1] + gaps[i];
        }

        // Decode BM25 scores
        std::vector<int> bm25ScoresQuantized = varbyteDecodeList(compressedBM25Scores);
        std::vector<double> bm25Scores(bm25ScoresQuantized.size());
        for (size_t i = 0; i < bm25ScoresQuantized.size(); ++i) {
            bm25Scores[i] = static_cast<double>(bm25ScoresQuantized[i]) / 1000.0;  // De-quantize
        }

        // Print postings list
        std::cout << "Term: " << term << ", Postings: ";
        for (size_t i = 0; i < docIDs.size(); ++i) {
            std::cout << "(DocID: " << docIDs[i] << ", BM25: " << bm25Scores[i] << ") ";
        }
        std::cout << std::endl;
    }

    indexFile.close();
    lexiconFile.close();
}

int main() {
    // Read and print the lexicon
    readAndPrintLexicon("../data/lexicon.bin");

    // Read and print the inverted index
    readAndPrintInvertedIndex("../data/index.bin", "../data/lexicon.bin");

    return 0;
}
