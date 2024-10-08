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

namespace fs = std::filesystem;

// Log messages to a file (for debugging purposes)
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
    std::priority_queue<std::tuple<std::string, int, int>, std::vector<std::tuple<std::string, int, int>>, decltype(cmp)> pq(cmp);

    // Initial read from each temp file
    for (int i = 0; i < numFiles; ++i) {
        if (tempFiles[i].peek() != EOF) {
            uint16_t termLength;
            tempFiles[i].read(reinterpret_cast<char*>(&termLength), sizeof(termLength));
            std::string term(termLength, ' ');
            tempFiles[i].read(&term[0], termLength);
            int docID;
            tempFiles[i].read(reinterpret_cast<char*>(&docID), sizeof(docID));

            pq.push(std::make_tuple(term, docID, i));
        }
    }

    std::string currentTerm;
    std::vector<int> docIDs;
    int64_t offset = 0;

    while (!pq.empty()) {
        auto [term, docID, fileIndex] = pq.top();
        pq.pop();

        if (currentTerm.empty()) {
            currentTerm = term;
        }

        if (term != currentTerm) {
            // Write postings list for currentTerm
            LexiconEntry entry;
            entry.offset = offset;
            entry.docFrequency = docIDs.size();

            // Compress docIDs
            std::vector<int> gaps(docIDs.size());
            gaps[0] = docIDs[0];
            for (size_t i = 1; i < docIDs.size(); ++i) {
                gaps[i] = docIDs[i] - docIDs[i - 1];
            }
            std::vector<unsigned char> compressedDocIDs = varbyteEncodeList(gaps);
            entry.length = compressedDocIDs.size();

            // Write to index file
            indexFile.write(reinterpret_cast<char*>(compressedDocIDs.data()), compressedDocIDs.size());
            offset += compressedDocIDs.size();

            // Update lexicon
            lexicon[currentTerm] = entry;

            // Reset for new term
            currentTerm = term;
            docIDs.clear();
        }

        docIDs.push_back(docID);

        // Read next term-docID pair from the same temp file
        if (tempFiles[fileIndex].peek() != EOF) {
            uint16_t termLength;
            tempFiles[fileIndex].read(reinterpret_cast<char*>(&termLength), sizeof(termLength));
            std::string nextTerm(termLength, ' ');
            tempFiles[fileIndex].read(&nextTerm[0], termLength);
            int nextDocID;
            tempFiles[fileIndex].read(reinterpret_cast<char*>(&nextDocID), sizeof(nextDocID));

            pq.push(std::make_tuple(nextTerm, nextDocID, fileIndex));
        }
    }

    // Write postings list for the last term
    if (!docIDs.empty()) {
        LexiconEntry entry;
        entry.offset = offset;
        entry.docFrequency = docIDs.size();

        // Compress docIDs
        std::vector<int> gaps(docIDs.size());
        gaps[0] = docIDs[0];
        for (size_t i = 1; i < docIDs.size(); ++i) {
            gaps[i] = docIDs[i] - docIDs[i - 1];
        }
        std::vector<unsigned char> compressedDocIDs = varbyteEncodeList(gaps);
        entry.length = compressedDocIDs.size();

        // Write to index file
        indexFile.write(reinterpret_cast<char*>(compressedDocIDs.data()), compressedDocIDs.size());
        offset += compressedDocIDs.size();

        // Update lexicon
        lexicon[currentTerm] = entry;
    }

    // Close files
    for (auto &file : tempFiles) {
        file.close();
    }
    indexFile.close();
    logMessage("Merging completed.");
}

// Write the lexicon to a binary file
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
