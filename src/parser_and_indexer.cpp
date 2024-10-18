#include "parser_and_indexer.h"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <ctime>
#include <cstdint>



// Log messages to a file (for debugging purposes)
std::ofstream logFile("../logs/parser.log", std::ios::app);
void logMessage(const std::string &message) {
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file!" << std::endl;
        return;
    }
    std::time_t currentTime = std::time(nullptr);
    logFile << std::asctime(std::localtime(&currentTime)) << message << std::endl;
}



// Function to process a single passage and generate term-docID pairs
void processPassage(int docID, const std::string &passage, std::vector<TermDocPair> &termDocPairs) {
    logMessage("Processing passage for docID: " + std::to_string(docID));
    auto terms = tokenize(passage);

    // For each term, generate a TermDocPair
    for (const auto &term : terms) {
        termDocPairs.push_back({term, docID});
    }
}



// Generate term-docID pairs using the specified algorithm
void generateTermDocPairs(const std::string &inputFile, std::unordered_map<int, std::string> &pageTable) {
    std::ifstream inputFileStream(inputFile);
    if (!inputFileStream.is_open()) {
        logMessage("Error opening input file: " + inputFile);
        return;
    }

    std::vector<TermDocPair> termDocPairs;
    std::string line;
    int docID = 0;
    int fileCounter = 0;

    while (std::getline(inputFileStream, line)) {
        size_t tabPos = line.find('\t');
        if (tabPos != std::string::npos) {
            std::string docName = line.substr(0, tabPos);
            std::string passage = line.substr(tabPos + 1);

            // Store in page table
            pageTable[docID] = docName;

            processPassage(docID++, passage, termDocPairs);

            // Check if termDocPairs reached MAX_RECORDS
            if (termDocPairs.size() >= MAX_RECORDS) {
                saveTermDocPairsToFile(termDocPairs, fileCounter++);
                termDocPairs.clear();  // Clear buffer after saving
            }
        }
    }

    // Save remaining termDocPairs
    if (!termDocPairs.empty()) {
        saveTermDocPairsToFile(termDocPairs, fileCounter++);
    }

    inputFileStream.close();
    logMessage("Term-DocID pair generation completed.");
}



#ifndef PARSER_AND_INDEXER_MT_H
#include <chrono>

int main() {
    auto startTime = std::chrono::high_resolution_clock::now();
    createDirectory("../data");
    createDirectory("../data/intermediate");

    // Data structures for the page table
    std::unordered_map<int, std::string> pageTable;

    // Generate term-docID pairs from the input collection
    generateTermDocPairs("../data/collection_short.tsv", pageTable);

    // Write the page table to file
    writePageTableToFile(pageTable);

    logMessage("Parsing process completed.");

    auto endTime = std::chrono::high_resolution_clock::now();

    std::cout << "time passed: " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << std::endl;


    return 0;
}

#endif
