#include "compression.h"
#include "merger.h"  // Include to access LexiconEntry
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

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
        int64_t offset;
        int length;
        int docFrequency;

        lexiconFile.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        lexiconFile.read(reinterpret_cast<char*>(&length), sizeof(length));
        lexiconFile.read(reinterpret_cast<char*>(&docFrequency), sizeof(docFrequency));

        // Print the lexicon entry
        std::cout << "Term: " << term << ", Offset: " << offset
                  << ", Length: " << length << ", DocFrequency: " << docFrequency << std::endl;
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
        int64_t offset;
        int length;
        int docFrequency;

        lexiconFile.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        lexiconFile.read(reinterpret_cast<char*>(&length), sizeof(length));
        lexiconFile.read(reinterpret_cast<char*>(&docFrequency), sizeof(docFrequency));

        // Read postings list from index file
        indexFile.seekg(offset, std::ios::beg);
        std::vector<unsigned char> compressedData(length);
        indexFile.read(reinterpret_cast<char*>(compressedData.data()), length);

        // Decode postings
        std::vector<int> gaps = varbyteDecodeList(compressedData);
        std::vector<int> docIDs(gaps.size());
        docIDs[0] = gaps[0];
        for (size_t i = 1; i < gaps.size(); ++i) {
            docIDs[i] = docIDs[i - 1] + gaps[i];
        }

        // Print postings list
        std::cout << "Term: " << term << ", Postings: ";
        for (const auto &docID : docIDs) {
            std::cout << docID << " ";
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
