// test_merger_output.cpp

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <iomanip>

// Lexicon entry structure
struct LexiconEntry {
    int64_t offset;
    int32_t length;
    int32_t docFrequency;
    int32_t blockCount;
    std::vector<int32_t> blockMaxDocIDs; // Maximum docID in each block
    std::vector<int64_t> blockOffsets;   // Offset of each block in the index file
};

// Function to read the lexicon from file
std::unordered_map<std::string, LexiconEntry> readLexicon(const std::string &lexiconFilename) {
    std::unordered_map<std::string, LexiconEntry> lexicon;

    std::ifstream lexiconFile(lexiconFilename, std::ios::binary);
    if (!lexiconFile.is_open()) {
        std::cerr << "Error opening lexicon file: " << lexiconFilename << std::endl;
        return lexicon;
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

    return lexicon;
}

// Function to read and display the index file contents
void displayIndex(const std::string &indexFilename, const std::unordered_map<std::string, LexiconEntry> &lexicon) {
    std::ifstream indexFile(indexFilename, std::ios::binary);
    if (!indexFile.is_open()) {
        std::cerr << "Error opening index file: " << indexFilename << std::endl;
        return;
    }

    for (const auto &[term, entry] : lexicon) {
        std::cout << "Term: " << term << std::endl;
        std::cout << "  Offset: " << entry.offset << std::endl;
        std::cout << "  Length: " << entry.length << std::endl;
        std::cout << "  Document Frequency: " << entry.docFrequency << std::endl;

        // Read the compressed postings list
        indexFile.seekg(entry.offset, std::ios::beg);
        std::vector<unsigned char> compressedData(entry.length);
        indexFile.read(reinterpret_cast<char*>(compressedData.data()), entry.length);

        // Decompress docID gaps
        size_t pos = 0;
        int lastDocID = 0;
        std::vector<int> docIDs;
        while (pos < compressedData.size()) {
            int gap = 0;
            int shift = 0;
            while (true) {
                unsigned char byte = compressedData[pos++];
                gap |= (byte & 0x7F) << shift;
                if ((byte & 0x80) == 0) break;
                shift += 7;
            }
            lastDocID += gap;
            docIDs.push_back(lastDocID);
        }

        // Display the postings list
        std::cout << "  Postings List (DocIDs): ";
        for (int docID : docIDs) {
            std::cout << docID << " ";
        }
        std::cout << std::endl << std::endl;
    }

    indexFile.close();
}

int main() {
    std::string lexiconFilename = "../data/lexicon.bin";
    std::string indexFilename = "../data/index.bin";

    // Read the lexicon
    auto lexicon = readLexicon(lexiconFilename);

    // Display the contents of the index file
    displayIndex(indexFilename, lexicon);

    return 0;
}
