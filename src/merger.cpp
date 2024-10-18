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
#include <cmath>  // For BM25 computations
#include <algorithm> // For std::sort

namespace fs = std::filesystem;

// Constants for BM25
const double k1 = 1.5;
const double b = 0.75;

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


void mergeTempFiles(int numFiles, std::unordered_map<std::string, LexiconEntry> &lexicon, int totalDocuments, double avgDocLength) {
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
    auto cmp = [](const std::tuple<std::string, int, int, int, int> &a, const std::tuple<std::string, int, int, int, int> &b) {
        if (std::get<0>(a) == std::get<0>(b)) {
            return std::get<1>(a) > std::get<1>(b);  // Compare docIDs
        }
        return std::get<0>(a) > std::get<0>(b);  // Compare terms
    };
    std::priority_queue<
        std::tuple<std::string, int, int, int, int>,
        std::vector<std::tuple<std::string, int, int, int, int>>,
        decltype(cmp)
    > pq(cmp);

    // Initial read from each temp file
    for (int i = 0; i < numFiles; ++i) {
        if (tempFiles[i].peek() != EOF) {
            uint16_t termLength;
            tempFiles[i].read(reinterpret_cast<char*>(&termLength), sizeof(termLength));
            std::string term(termLength, ' ');
            tempFiles[i].read(&term[0], termLength);
            int docID;
            tempFiles[i].read(reinterpret_cast<char*>(&docID), sizeof(docID));
            int tf;
            tempFiles[i].read(reinterpret_cast<char*>(&tf), sizeof(tf));
            int docLength;
            tempFiles[i].read(reinterpret_cast<char*>(&docLength), sizeof(docLength));

            pq.push(std::make_tuple(term, docID, tf, docLength, i));
        }
    }

    std::string currentTerm;
    std::vector<std::tuple<int, int, int>> postingsList;  // Stores (docID, tf, docLength)
    int64_t offset = 0;

    while (!pq.empty()) {
        auto [term, docID, tf, docLength, fileIndex] = pq.top();
        pq.pop();

        if (currentTerm.empty()) {
            currentTerm = term;
        }

        if (term != currentTerm) {
            // Process postingsList for currentTerm
            int df = postingsList.size();

            // Compute BM25 scores
            std::vector<std::tuple<int, double>> postingsWithBM25;  // (docID, BM25 score)
            for (const auto& [docID, tf, docLength] : postingsList) {
                double idf = std::log((totalDocuments - df + 0.5) / (df + 0.5));
                double K = k1 * ((1 - b) + b * (docLength / avgDocLength));
                double bm25Score = idf * ((k1 + 1) * tf) / (K + tf);
                postingsWithBM25.emplace_back(docID, bm25Score);
            }

            // Sort postings by docID to ensure order
            std::sort(postingsWithBM25.begin(), postingsWithBM25.end());

            // Prepare data for compression
            std::vector<int> docIDGaps(postingsWithBM25.size());
            std::vector<int> bm25ScoresQuantized(postingsWithBM25.size());

            // Compute gaps and quantize BM25 scores
            docIDGaps[0] = std::get<0>(postingsWithBM25[0]);
            bm25ScoresQuantized[0] = static_cast<int>(std::get<1>(postingsWithBM25[0]) * 1000);  // Quantize BM25 score
            for (size_t i = 1; i < postingsWithBM25.size(); ++i) {
                docIDGaps[i] = std::get<0>(postingsWithBM25[i]) - std::get<0>(postingsWithBM25[i - 1]);
                bm25ScoresQuantized[i] = static_cast<int>(std::get<1>(postingsWithBM25[i]) * 1000);
            }

            // Compress docID gaps and BM25 scores
            std::vector<unsigned char> compressedDocIDs = varbyteEncodeList(docIDGaps);
            std::vector<unsigned char> compressedBM25Scores = varbyteEncodeList(bm25ScoresQuantized);

            LexiconEntry entry;
            entry.offset = offset;
            entry.docFrequency = df;
            entry.docIDsLength = compressedDocIDs.size();
            entry.length = compressedDocIDs.size() + compressedBM25Scores.size();

            // Write to index file
            indexFile.write(reinterpret_cast<char*>(compressedDocIDs.data()), compressedDocIDs.size());
            indexFile.write(reinterpret_cast<char*>(compressedBM25Scores.data()), compressedBM25Scores.size());
            offset += entry.length;

            // Update lexicon
            lexicon[currentTerm] = entry;

            // Reset for new term
            currentTerm = term;
            postingsList.clear();
        }

        // Add current posting to postingsList
        postingsList.emplace_back(docID, tf, docLength);

        // Read next term-docID-tf-dl tuple from the same temp file
        if (tempFiles[fileIndex].peek() != EOF) {
            uint16_t termLength;
            tempFiles[fileIndex].read(reinterpret_cast<char*>(&termLength), sizeof(termLength));
            std::string nextTerm(termLength, ' ');
            tempFiles[fileIndex].read(&nextTerm[0], termLength);
            int nextDocID;
            tempFiles[fileIndex].read(reinterpret_cast<char*>(&nextDocID), sizeof(nextDocID));
            int nextTf;
            tempFiles[fileIndex].read(reinterpret_cast<char*>(&nextTf), sizeof(nextTf));
            int nextDocLength;
            tempFiles[fileIndex].read(reinterpret_cast<char*>(&nextDocLength), sizeof(nextDocLength));

            pq.push(std::make_tuple(nextTerm, nextDocID, nextTf, nextDocLength, fileIndex));
        }
    }

    // Process postingsList for the last term
    if (!postingsList.empty()) {
        int df = postingsList.size();

        // Compute BM25 scores
        std::vector<std::tuple<int, double>> postingsWithBM25;  // (docID, BM25 score)
        for (const auto& [docID, tf, docLength] : postingsList) {
            double idf = std::log((totalDocuments - df + 0.5) / (df + 0.5));
            double K = k1 * ((1 - b) + b * (docLength / avgDocLength));
            double bm25Score = idf * ((k1 + 1) * tf) / (K + tf);
            postingsWithBM25.emplace_back(docID, bm25Score);
        }

        // Sort postings by docID to ensure order
        std::sort(postingsWithBM25.begin(), postingsWithBM25.end());

        // Prepare data for compression
        std::vector<int> docIDGaps(postingsWithBM25.size());
        std::vector<int> bm25ScoresQuantized(postingsWithBM25.size());

        // Compute gaps and quantize BM25 scores
        docIDGaps[0] = std::get<0>(postingsWithBM25[0]);
        bm25ScoresQuantized[0] = static_cast<int>(std::get<1>(postingsWithBM25[0]) * 1000);  // Quantize BM25 score
        for (size_t i = 1; i < postingsWithBM25.size(); ++i) {
            docIDGaps[i] = std::get<0>(postingsWithBM25[i]) - std::get<0>(postingsWithBM25[i - 1]);
            bm25ScoresQuantized[i] = static_cast<int>(std::get<1>(postingsWithBM25[i]) * 1000);
        }

        // Compress docID gaps and BM25 scores
        std::vector<unsigned char> compressedDocIDs = varbyteEncodeList(docIDGaps);
        std::vector<unsigned char> compressedBM25Scores = varbyteEncodeList(bm25ScoresQuantized);

        LexiconEntry entry;
        entry.offset = offset;
        entry.docFrequency = df;
        entry.docIDsLength = compressedDocIDs.size();
        entry.length = compressedDocIDs.size() + compressedBM25Scores.size();

        // Write to index file
        indexFile.write(reinterpret_cast<char*>(compressedDocIDs.data()), compressedDocIDs.size());
        indexFile.write(reinterpret_cast<char*>(compressedBM25Scores.data()), compressedBM25Scores.size());
        offset += entry.length;

        // Update lexicon
        lexicon[currentTerm] = entry;
    }

    // Close files
    for (auto &file : tempFiles) {
        file.close();
    }
    indexFile.close();
    logMessage("Merging completed with BM25 scores.");
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
        lexiconFile.write(reinterpret_cast<const char*>(&entry.docIDsLength), sizeof(entry.docIDsLength));
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

    // Calculate total number of documents and average document length
    int totalDocuments = 0;
    double totalDocLength = 0.0;
    std::unordered_map<int, int> docLengths;  // Map of docID to document length

    // Read temp files to collect document lengths
    for (int i = 0; i < numTempFiles; ++i) {
        std::ifstream tempFile("../data/intermediate/temp" + std::to_string(i) + ".bin", std::ios::binary);
        if (!tempFile.is_open()) {
            logMessage("Error opening temp file for document length calculation.");
            return 1;
        }

        while (tempFile.peek() != EOF) {
            uint16_t termLength;
            tempFile.read(reinterpret_cast<char*>(&termLength), sizeof(termLength));
            std::string term(termLength, ' ');
            tempFile.read(&term[0], termLength);
            int docID;
            tempFile.read(reinterpret_cast<char*>(&docID), sizeof(docID));
            int tf;
            tempFile.read(reinterpret_cast<char*>(&tf), sizeof(tf));
            int docLength;
            tempFile.read(reinterpret_cast<char*>(&docLength), sizeof(docLength));

            // Collect document lengths
            docLengths[docID] = docLength;

            // Skip to next entry (if there are any additional fields)
            // Assuming no additional fields, else adjust accordingly
        }

        tempFile.close();
    }

    // Compute total documents and average document length
    totalDocuments = docLengths.size();
    for (const auto &[docID, length] : docLengths) {
        totalDocLength += length;
    }
    double avgDocLength = totalDocLength / totalDocuments;

    std::unordered_map<std::string, LexiconEntry> lexicon;

    // Merge temp files to create the inverted index and lexicon with BM25 scores
    mergeTempFiles(numTempFiles, lexicon, totalDocuments, avgDocLength);

    // Write the lexicon to file
    writeLexiconToFile(lexicon);

    logMessage("Merging process completed with BM25 scores.");

    return 0;
}


