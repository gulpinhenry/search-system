#include "parser_and_indexer.h"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <sys/stat.h>
#include <ctime>
#include <cstdint>
#include <algorithm>

#ifdef _WIN32
    #include <direct.h>  // For _mkdir on Windows
    #define mkdir _mkdir
#else
    #include <sys/stat.h>  // For mkdir on Unix
#endif

#define MAX_RECORDS 1000000  // Maximum number of term-docID pairs in memory

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

// Helper function to create directories for data 
void createDirectory(const std::string &dir) {
    struct stat info;

    if (stat(dir.c_str(), &info) != 0) {
        // Directory does not exist
        #ifdef _WIN32
            _mkdir(dir.c_str());
        #else
            mkdir(dir.c_str(), 0777);  // Make directory with full permissions
        #endif
    } else if (!(info.st_mode & S_IFDIR)) {
        std::cerr << "Error: " << dir << " exists but is not a directory!" << std::endl;
    }
}

// Tokenize the given text into terms, convert to lowercase, remove punctuation
std::vector<std::string> tokenize(const std::string &text) {
    std::vector<std::string> tokens;
    std::istringstream iss(text);
    std::string word;
    while (iss >> word) {
        // Remove punctuation from word and convert to lowercase
        word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        if (!word.empty()) {
            tokens.push_back(word);
        }
    }
    return tokens;
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

// Save term-docID pairs to a temp file
void saveTermDocPairsToFile(const std::vector<TermDocPair> &termDocPairs, int &fileCounter) {
    // Sort the termDocPairs by term and docID
    std::vector<TermDocPair> sortedPairs = termDocPairs;
    std::sort(sortedPairs.begin(), sortedPairs.end(), [](const TermDocPair &a, const TermDocPair &b) {
        if (a.term == b.term) {
            return a.docID < b.docID;
        }
        return a.term < b.term;
    });

    // Create a new temp file
    std::ofstream tempFile("../data/intermediate/temp" + std::to_string(fileCounter++) + ".bin", std::ios::binary);
    if (!tempFile.is_open()) {
        logMessage("Error opening temp file for writing.");
        return;
    }

    // Write term-docID pairs to the file
    for (const auto &pair : sortedPairs) {
        uint16_t termLength = static_cast<uint16_t>(pair.term.length());
        tempFile.write(reinterpret_cast<const char*>(&termLength), sizeof(termLength));
        tempFile.write(pair.term.c_str(), termLength);
        tempFile.write(reinterpret_cast<const char*>(&pair.docID), sizeof(pair.docID));
    }

    tempFile.close();
    logMessage("Saved term-docID pairs to temp file.");
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
                saveTermDocPairsToFile(termDocPairs, fileCounter);
                termDocPairs.clear();  // Clear buffer after saving
            }
        }
    }

    // Save remaining termDocPairs
    if (!termDocPairs.empty()) {
        saveTermDocPairsToFile(termDocPairs, fileCounter);
    }

    inputFileStream.close();
    logMessage("Term-DocID pair generation completed.");
}

// Write the page table to a binary file
void writePageTableToFile(const std::unordered_map<int, std::string> &pageTable) {
    std::ofstream pageTableFile("../data/page_table.bin", std::ios::binary);
    if (!pageTableFile.is_open()) {
        logMessage("Error opening page table file for writing.");
        return;
    }

    for (const auto &[docID, docName] : pageTable) {
        // Write docID
        pageTableFile.write(reinterpret_cast<const char*>(&docID), sizeof(docID));

        // Write docName length and docName
        uint16_t nameLength = static_cast<uint16_t>(docName.length());
        pageTableFile.write(reinterpret_cast<const char*>(&nameLength), sizeof(nameLength));
        pageTableFile.write(docName.c_str(), nameLength);
    }

    pageTableFile.close();
    logMessage("Page table written to file.");
}

int main() {
    createDirectory("../data");
    createDirectory("../data/intermediate");

    // Data structures for the page table
    std::unordered_map<int, std::string> pageTable;

    // Generate term-docID pairs from the input collection
    generateTermDocPairs("../data/collection.tsv", pageTable);

    // Write the page table to file
    writePageTableToFile(pageTable);

    logMessage("Parsing process completed.");

    return 0;
}
