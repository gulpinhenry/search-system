#include "utils.h"
#include <sys/stat.h>
#include <iostream>

#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>

#include <cstdint>



#ifdef _WIN32
    #include <direct.h>  // For _mkdir on Windows
    #define mkdir _mkdir
#else
    #include <sys/stat.h>  // For mkdir on Unix
#endif

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
extern void logMessage(const std::string &message);

// Save term-docID pairs to a temp file
void saveTermDocPairsToFile(const std::vector<TermDocPair> &termDocPairs, const int &fileCounter) {
    // Sort the termDocPairs by term and docID
    std::vector<TermDocPair> sortedPairs = termDocPairs;
    std::sort(sortedPairs.begin(), sortedPairs.end(), [](const TermDocPair &a, const TermDocPair &b) {
        if (a.term == b.term) {
            return a.docID < b.docID;
        }
        return a.term < b.term;
    });

    // Create a new temp file
    std::ofstream tempFile("../data/intermediate/temp" + std::to_string(fileCounter) + ".bin", std::ios::binary);
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
        tempFile.write(reinterpret_cast<const char*>(&pair.termFScore), sizeof(pair.termFScore));
    }

    tempFile.close();
    logMessage("Saved term-docID pairs to temp file.");
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

void writeDocLengthsToFile(const std::unordered_map<int, int> &docLengths) {
    std::ofstream docLengthsFile("../data/doc_lengths.bin", std::ios::binary);
    if (!docLengthsFile.is_open()) {
        logMessage("Error opening doc_lengths.bin for writing.");
        return;
    }

    for (const auto &[docID, docLength] : docLengths) {
        docLengthsFile.write(reinterpret_cast<const char *>(&docID), sizeof(docID));
        docLengthsFile.write(reinterpret_cast<const char *>(&docLength), sizeof(docLength));
    }

    docLengthsFile.close();
    logMessage("Document lengths written to doc_lengths.bin.");
}
