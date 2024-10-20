// merger.cpp
#include "merger.h"
#include "compression.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <filesystem>
#include <ctime>
#include <algorithm> 

namespace fs = std::filesystem;

// number of postings per block
const int BLOCK_SIZE = 128;

std::ofstream logFile("../logs/merge.log", std::ios::app);

void logMessage(const std::string &message) {
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file!" << std::endl;
        return;
    }
    std::time_t currentTime = std::time(nullptr);
    logFile << std::asctime(std::localtime(&currentTime)) << message << std::endl;
}

void mergeTempFiles(int numFiles, std::unordered_map<std::string, LexiconEntry> &lexicon) {
    // Open all temp files
    std::vector<std::ifstream> tempFiles(numFiles);
    for (int i = 0; i < numFiles; ++i) {
        tempFiles[i].open("../data/intermediate/temp" + std::to_string(i) + ".bin", std::ios::binary);
        if (!tempFiles[i].is_open()) {
            logMessage("Error opening temp file for merging.");
            return;
        }
    }

    // Output index file
    std::ofstream indexFile("../data/index.bin", std::ios::binary);
    if (!indexFile.is_open()) {
        logMessage("Error opening index file for writing.");
        return;
    }

    // Priority queue for merging
    auto cmp = [](const std::tuple<std::string, int, int> &a, const std::tuple<std::string, int, int> &b) {
        if (std::get<0>(a) == std::get<0>(b)) {
            return std::get<1>(a) > std::get<1>(b);  // Compare docIDs
        }
        return std::get<0>(a) > std::get<0>(b);  // Compare terms
    };
    std::priority_queue<
        std::tuple<std::string, int, int>,
        std::vector<std::tuple<std::string, int, int>>,
        decltype(cmp)
    > pq(cmp);

    // Initial read from each temp file
    for (int i = 0; i < numFiles; ++i) {
        if (tempFiles[i].peek() != EOF) {
            uint16_t termLength;
            tempFiles[i].read(reinterpret_cast<char*>(&termLength), sizeof(termLength));
            if (!tempFiles[i]) continue; // Error or EOF
            std::vector<char> termBuffer(termLength);
            tempFiles[i].read(termBuffer.data(), termLength);
            if (!tempFiles[i]) continue; // Error or EOF
            std::string term(termBuffer.begin(), termBuffer.end());

            int docID;
            tempFiles[i].read(reinterpret_cast<char*>(&docID), sizeof(docID));
            if (!tempFiles[i]) continue; // Error or EOF

            pq.push(std::make_tuple(term, docID, i));
        }
    }

    std::string currentTerm;
    std::vector<int> postingsList;  // Stores docIDs
    int64_t offset = 0;

    while (!pq.empty()) {
        auto [term, docID, fileIndex] = pq.top();
        pq.pop();

        if (currentTerm.empty()) {
            currentTerm = term;
        }

        if (term != currentTerm) {
            // Process postingsList for currentTerm
            int df = postingsList.size();

            // Sort postings by docID to ensure order
            std::sort(postingsList.begin(), postingsList.end());

            // Compute docID gaps
            std::vector<int> docIDGaps(postingsList.size());
            int lastDocID = 0;
            for (size_t i = 0; i < postingsList.size(); ++i) {
                docIDGaps[i] = postingsList[i] - lastDocID;
                lastDocID = postingsList[i];
            }

            // Compress docID gaps
            std::vector<unsigned char> compressedDocIDs = varbyteEncodeList(docIDGaps);

            // Write to index file
            int64_t offsetBefore = offset;
            indexFile.write(reinterpret_cast<char*>(compressedDocIDs.data()), compressedDocIDs.size());
            offset += compressedDocIDs.size();

            // Update lexicon
            LexiconEntry entry;
            entry.offset = offsetBefore;
            entry.length = compressedDocIDs.size();
            entry.docFrequency = df;
            entry.blockCount = 0; // Not using blocking in this version

            lexicon[currentTerm] = entry;

            // Reset for new term
            currentTerm = term;
            postingsList.clear();
        }

        // Add current posting to postingsList
        postingsList.push_back(docID);

        // Read next term-docID tuple from the same temp file
        if (tempFiles[fileIndex].peek() != EOF) {
            uint16_t termLength;
            tempFiles[fileIndex].read(reinterpret_cast<char*>(&termLength), sizeof(termLength));
            if (!tempFiles[fileIndex]) continue; // Error or EOF
            std::vector<char> termBuffer(termLength);
            tempFiles[fileIndex].read(termBuffer.data(), termLength);
            if (!tempFiles[fileIndex]) continue; // Error or EOF
            std::string nextTerm(termBuffer.begin(), termBuffer.end());

            int nextDocID;
            tempFiles[fileIndex].read(reinterpret_cast<char*>(&nextDocID), sizeof(nextDocID));
            if (!tempFiles[fileIndex]) continue; // Error or EOF

            pq.push(std::make_tuple(nextTerm, nextDocID, fileIndex));
        }
    }

    // Process postingsList for the last term
    if (!postingsList.empty()) {
        int df = postingsList.size();

        // Sort postings by docID to ensure order
        std::sort(postingsList.begin(), postingsList.end());

        // Compute docID gaps
        std::vector<int> docIDGaps(postingsList.size());
        int lastDocID = 0;
        for (size_t i = 0; i < postingsList.size(); ++i) {
            docIDGaps[i] = postingsList[i] - lastDocID;
            lastDocID = postingsList[i];
        }

        // Compress docID gaps
        std::vector<unsigned char> compressedDocIDs = varbyteEncodeList(docIDGaps);

        // Write to index file
        int64_t offsetBefore = offset;
        indexFile.write(reinterpret_cast<char*>(compressedDocIDs.data()), compressedDocIDs.size());
        offset += compressedDocIDs.size();

        // Update lexicon
        LexiconEntry entry;
        entry.offset = offsetBefore;
        entry.length = compressedDocIDs.size();
        entry.docFrequency = df;
        entry.blockCount = 0; // Not using blocking in this version

        lexicon[currentTerm] = entry;
    }

    // Close files
    for (auto &file : tempFiles) {
        file.close();
    }
    indexFile.close();
    logMessage("Merging completed.");
}

// Function to write the lexicon to a file
void writeLexiconToFile(const std::unordered_map<std::string, LexiconEntry> &lexicon) {
    std::ofstream lexiconFile("../data/lexicon.bin", std::ios::binary);
    if (!lexiconFile.is_open()) {
        logMessage("Error opening lexicon file for writing.");
        return;
    }

    for (const auto &[term, entry] : lexicon) {
        // Write term length and term
        uint16_t termLength = static_cast<uint16_t>(term.length());
        lexiconFile.write(reinterpret_cast<const char*>(&termLength), sizeof(termLength));
        lexiconFile.write(term.c_str(), termLength);

        // Write LexiconEntry data
        lexiconFile.write(reinterpret_cast<const char*>(&entry.offset), sizeof(entry.offset));
        lexiconFile.write(reinterpret_cast<const char*>(&entry.length), sizeof(entry.length));
        lexiconFile.write(reinterpret_cast<const char*>(&entry.docFrequency), sizeof(entry.docFrequency));

        // Since we're not using blocking, set blockCount to 0
        lexiconFile.write(reinterpret_cast<const char*>(&entry.blockCount), sizeof(entry.blockCount));
    }

    lexiconFile.close();
    logMessage("Lexicon written to file.");
}

int main() {
    // Read the number of temp files generated
    int numTempFiles = 0;
    while (true) {
        std::string tempFileName = "../data/intermediate/temp" + std::to_string(numTempFiles) + ".bin";
        if (!fs::exists(tempFileName)) {
            break;
        }
        numTempFiles++;
    }

    if (numTempFiles == 0) {
        std::cerr << "No temp files found for merging." << std::endl;
        return 1;
    }

    std::unordered_map<std::string, LexiconEntry> lexicon;

    // Merge temp files to create the inverted index and lexicon
    mergeTempFiles(numTempFiles, lexicon);

    // Write the lexicon to file
    writeLexiconToFile(lexicon);

    logMessage("Merging process completed.");

    return 0;
}
