#include "merger.h"
#include "compression.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <unordered_map>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <regex>
#include <ctime>
#include <cerrno>  // for errno
#include <cstring> // for strerror

namespace fs = std::filesystem;

#define BLOCK_SIZE 64  // Number of postings per block
#define MAX_OPEN_FILES 10  // Maximum number of files to be opened at the same time

// Log file
std::ofstream logFile("../logs/merger.log", std::ios::app);

// Function to log messages to the log file with a timestamp
void logMessage(const std::string &message) {
    std::time_t currentTime = std::time(nullptr);
    logFile << std::asctime(std::localtime(&currentTime)) << message << std::endl;
}


// Function to load the next posting from the file and push it into the heap
bool loadNextPosting(std::ifstream &file, int fileIndex, std::priority_queue<PostingEntry, std::vector<PostingEntry>, std::greater<>> &minHeap) {
    std::vector<unsigned char> docIDBuffer;
    std::vector<unsigned char> freqBuffer;
    int blockSize = BLOCK_SIZE * 2; // assuming blocks of docID and frequency are equal in size

    // Read binary data for docIDs and frequencies
    docIDBuffer.resize(blockSize);
    freqBuffer.resize(blockSize);

    // Read the docIDs
    if (!file.read(reinterpret_cast<char*>(docIDBuffer.data()), blockSize)) {
        logMessage("Error reading docIDs from fileIndex: " + std::to_string(fileIndex) + ". Possible format issue.");
        return false;
    }

    // Read the frequencies
    if (!file.read(reinterpret_cast<char*>(freqBuffer.data()), blockSize)) {
        logMessage("Error reading frequencies from fileIndex: " + std::to_string(fileIndex) + ". Possible format issue.");
        return false;
    }

    // Decode docIDs and frequencies
    std::vector<int> docIDs = varbyteDecodeList(docIDBuffer);
    std::vector<int> frequencies = varbyteDecodeList(freqBuffer);

    // Push each posting into the heap
    for (size_t i = 0; i < docIDs.size(); ++i) {
        minHeap.push({"term_placeholder", docIDs[i], frequencies[i], fileIndex});
        logMessage("Loaded posting: docID=" + std::to_string(docIDs[i]) + ", freq=" + std::to_string(frequencies[i]));
    }

    return true;
}

// Function to compress and write a block of postings to the output file
void writeCompressedBlock(std::ofstream &binFile, const std::vector<int> &docIDs, const std::vector<int> &frequencies, 
                          std::vector<int> &lastdocid, std::vector<int> &docidsize, std::vector<int> &freqsize) {
    std::vector<unsigned char> compressedDocIDs = varbyteEncodeList(docIDs);
    std::vector<unsigned char> compressedFrequencies = varbyteEncodeList(frequencies);

    binFile.write(reinterpret_cast<char*>(compressedDocIDs.data()), compressedDocIDs.size());
    binFile.write(reinterpret_cast<char*>(compressedFrequencies.data()), compressedFrequencies.size());

    lastdocid.push_back(docIDs.back());
    docidsize.push_back(compressedDocIDs.size());
    freqsize.push_back(compressedFrequencies.size());
}

// Helper function to extract the numeric part from a filename like "temp123.bin"
int extractFileNumber(const std::string &filename) {
    std::regex pattern("temp(\\d+)\\.bin");
    std::smatch match;
    if (std::regex_search(filename, match, pattern)) {
        return std::stoi(match[1]);
    }
    return -1;
}

// Function to merge a batch of sorted files into a final compressed index
void mergeSortedFilesBatch(const std::vector<std::string> &inputFiles, const std::string &outputFile) {
    logMessage("Starting batch merge process...");

    std::ofstream binFile(outputFile, std::ios::binary);
    if (!binFile.is_open()) {
        logMessage("Error opening output file: " + outputFile);
        return;
    }

    std::vector<int> lastdocid, docidsize, freqsize;
    std::vector<int> docIDs, frequencies;

    std::priority_queue<PostingEntry, std::vector<PostingEntry>, std::greater<>> minHeap;

    for (size_t batchStart = 0; batchStart < inputFiles.size(); batchStart += MAX_OPEN_FILES) {
        size_t batchEnd = std::min(batchStart + MAX_OPEN_FILES, inputFiles.size());

        std::vector<std::ifstream> inputStreams(batchEnd - batchStart);
        for (size_t i = batchStart; i < batchEnd; ++i) {
            logMessage("Attempting to open file: " + inputFiles[i]);
            inputStreams[i - batchStart].open(inputFiles[i]);
            if (!inputStreams[i - batchStart].is_open()) {
                std::string errorMessage = "Error opening file: " + inputFiles[i] + " - " + std::strerror(errno);
                logMessage(errorMessage);
                continue;
            }
        }

        for (size_t i = 0; i < inputStreams.size(); ++i) {
            if (!loadNextPosting(inputStreams[i], i + batchStart, minHeap)) {
                logMessage("Error loading initial posting from file: " + inputFiles[i + batchStart]);
            }
        }

        while (!minHeap.empty()) {
            PostingEntry current = minHeap.top();
            minHeap.pop();

            docIDs.push_back(current.docID);
            frequencies.push_back(current.frequency);

            if (docIDs.size() == BLOCK_SIZE) {
                writeCompressedBlock(binFile, docIDs, frequencies, lastdocid, docidsize, freqsize);
                docIDs.clear();
                frequencies.clear();
            }

            if (!loadNextPosting(inputStreams[current.fileIndex - batchStart], current.fileIndex, minHeap)) {
                inputStreams[current.fileIndex - batchStart].close();
            }
        }
    }

    if (!docIDs.empty()) {
        writeCompressedBlock(binFile, docIDs, frequencies, lastdocid, docidsize, freqsize);
    }

    std::ofstream metaFile(outputFile + "_metadata.bin", std::ios::binary);
    for (size_t i = 0; i < lastdocid.size(); ++i) {
        metaFile.write(reinterpret_cast<const char*>(&lastdocid[i]), sizeof(int));
        metaFile.write(reinterpret_cast<const char*>(&docidsize[i]), sizeof(int));
        metaFile.write(reinterpret_cast<const char*>(&freqsize[i]), sizeof(int));
    }
    metaFile.close();

    binFile.close();
    logMessage("Batch merge completed and final index written to: " + outputFile);
}

int main() {
    // Directory containing the intermediate files
    std::string directory = "../data/intermediate/";
    std::vector<std::string> inputFiles;

    // Iterate over all files in the directory and select the ones that match our pattern (e.g., temp*.bin)
    for (const auto &entry : fs::directory_iterator(directory)) {
        std::string filename = entry.path().string();
        if (filename.find("temp") != std::string::npos && filename.find(".bin") != std::string::npos) {
            inputFiles.push_back(filename);
        }
    }

    // Sort the inputFiles numerically based on the number part of the filename
    std::sort(inputFiles.begin(), inputFiles.end(), [](const std::string &a, const std::string &b) {
        return extractFileNumber(a) < extractFileNumber(b);
    });

    // Output file for the final compressed index
    std::string outputFile = "../data/final_compressed_index.bin";

    // Check if we found any input files
    if (inputFiles.empty()) {
        logMessage("No intermediate files found in the directory: " + directory);
        return 1;  // Exit with an error code
    }

    // Log the files to be merged
    logMessage("Merging the following files:");
    for (const auto &file : inputFiles) {
        logMessage(" - " + file);
    }

    // Start the merge process (called only once)
    logMessage("Starting merge process...");
    mergeSortedFilesBatch(inputFiles, outputFile);

    return 0;  // End of main function
}
