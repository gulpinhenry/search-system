#include "parser_and_indexer.h"
#include "compression.h"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <queue>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <sys/stat.h>
#include <ctime>
#ifdef _WIN32
    #include <direct.h>  // For _mkdir on Windows
    #define mkdir _mkdir
#else
    #include <sys/stat.h>  // For mkdir on Unix
#endif

#define MAX_POSTINGS_PER_FILE 4096 // Maximum number of postings per file

// Log messages to a file (for debugging purposes)
std::ofstream logFile("../logs/parser_and_indexer.log", std::ios::app);
void logMessage(const std::string &message) {
    std::time_t currentTime = std::time(nullptr);
    logFile << std::asctime(std::localtime(&currentTime)) << message << std::endl;
}

// Helper function to create directories
void createDirectory(const std::string &dir) {
    mkdir(dir.c_str());
}

// Tokenize the given text into terms
std::vector<std::string> tokenize(const std::string &text) {
    std::istringstream iss(text);
    return std::vector<std::string>(std::istream_iterator<std::string>{iss},
                                    std::istream_iterator<std::string>{});
}

// Function to process a single passage and generate postings
void processPassage(int docID, const std::string &passage, std::unordered_map<std::string, std::vector<Posting>> &index) {
    logMessage("Processing passage for docID: " + std::to_string(docID));
    auto terms = tokenize(passage);
    std::unordered_map<std::string, int> termFrequency;

    // Count the frequency of each term in the document
    for (const auto &term : terms) {
        ++termFrequency[term];
    }

    // Add postings to the index
    for (const auto &[term, freq] : termFrequency) {
        index[term].push_back({docID, freq});
    }
}

// Writes a block of compressed postings to a binary file and updates metadata
void writeCompressedBlock(std::ofstream &binFile, const std::vector<int> &docIDs, const std::vector<int> &frequencies, 
                          std::vector<int> &lastdocid, std::vector<int> &docidsize, std::vector<int> &freqsize) {
    // Compress docIDs and frequencies using varbyte encoding
    std::vector<unsigned char> compressedDocIDs = varbyteEncodeList(docIDs);
    std::vector<unsigned char> compressedFrequencies = varbyteEncodeList(frequencies);

    // Write compressed docIDs and frequencies to the binary file
    binFile.write(reinterpret_cast<char*>(compressedDocIDs.data()), compressedDocIDs.size());
    binFile.write(reinterpret_cast<char*>(compressedFrequencies.data()), compressedFrequencies.size());

    // Update metadata
    if (!docIDs.empty()) {
        lastdocid.push_back(docIDs.back());               // Last docID in the block
        docidsize.push_back(compressedDocIDs.size());     // Size of compressed docID block
        freqsize.push_back(compressedFrequencies.size()); // Size of compressed frequency block
    }
}

// Save postings in files, limiting to a maximum of MAX_POSTINGS_PER_FILE postings per file
void savePostingsToFile(const std::unordered_map<std::string, std::vector<Posting>> &postingsBuffer, int &fileCounter) {
    // Sort the postings by term and docID
    std::vector<std::pair<std::string, std::vector<Posting>>> sortedPostings(postingsBuffer.begin(), postingsBuffer.end());
    std::sort(sortedPostings.begin(), sortedPostings.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });

    // Create a new file for this chunk
    std::ofstream binFile("../data/intermediate/temp" + std::to_string(fileCounter++) + ".bin", std::ios::binary);
    if (!binFile.is_open()) {
        logMessage("Error opening binary file for writing.");
        return;
    }

    std::vector<int> lastdocid, docidsize, freqsize;  // Metadata arrays

    std::vector<int> docIDs, frequencies;  // Buffer for the current block of postings
    int postingsCount = 0;

    for (const auto &[term, postings] : sortedPostings) {
        for (const auto &posting : postings) {
            docIDs.push_back(posting.docID);
            frequencies.push_back(posting.frequency);
            postingsCount++;

            // When the max number of postings is reached, write to the file and start a new one
            if (postingsCount == MAX_POSTINGS_PER_FILE) {
                writeCompressedBlock(binFile, docIDs, frequencies, lastdocid, docidsize, freqsize);
                docIDs.clear();
                frequencies.clear();
                postingsCount = 0;  // Reset the counter for new postings
            }
        }
    }

    // Write any remaining postings that don't fill up the max posting count
    if (!docIDs.empty()) {
        writeCompressedBlock(binFile, docIDs, frequencies, lastdocid, docidsize, freqsize);
    }

    binFile.close();
    logMessage("Finished writing to binary file.");
}

// Generate postings, sort them, and write them out in chunks of MAX_POSTINGS_PER_FILE postings
void generateCompressedIndex(const std::string &inputFile) {
    std::ifstream inputFileStream(inputFile);
    if (!inputFileStream.is_open()) {
        logMessage("Error opening input file: " + inputFile);
        return;
    }

    std::unordered_map<std::string, std::vector<Posting>> postingsBuffer;
    std::string line;
    int docID = 0;
    int fileCounter = 0;

    while (std::getline(inputFileStream, line)) {
        size_t tabPos = line.find('\t');
        if (tabPos != std::string::npos) {
            std::string passage = line.substr(tabPos + 1);
            processPassage(docID++, passage, postingsBuffer);
        }
    }

    // Save postings in chunks of 64
    savePostingsToFile(postingsBuffer, fileCounter);

    inputFileStream.close();
    logMessage("Compressed index generation completed.");
}

int main() {
    createDirectory("../data");
    createDirectory("../data/intermediate");

    // Generate compressed index from the input collection
    generateCompressedIndex("../data/collection.tsv");

    logMessage("Compressed index generation completed.");
    return 0;
}
