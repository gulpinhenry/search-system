#include "compression.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

// Function to read and decode a binary file (docIDs and frequencies) and print the contents
void readAndPrintIndexFile(const std::string &filename) {
    std::ifstream binFile(filename, std::ios::binary);
    if (!binFile.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    // Read the whole file into a buffer
    std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(binFile)), std::istreambuf_iterator<char>());

    // Close the binary file
    binFile.close();

    // Split the buffer into docID and frequency blocks (assuming the format from your indexer)
    size_t block_size = buffer.size() / 2;

    std::vector<unsigned char> docIDBlock(buffer.begin(), buffer.begin() + block_size);
    std::vector<unsigned char> freqBlock(buffer.begin() + block_size, buffer.end());

    // Decode the docID and frequency blocks
    std::vector<int> docIDs = varbyteDecodeList(docIDBlock);
    std::vector<int> frequencies = varbyteDecodeList(freqBlock);

    // Print the decoded docIDs and frequencies
    std::cout << "Decoded docIDs and frequencies from file: " << filename << std::endl;
    for (size_t i = 0; i < docIDs.size(); ++i) {
        std::cout << "DocID: " << docIDs[i] << ", Frequency: " << frequencies[i] << std::endl;
    }
}

int main() {
    // Path to the binary file you want to test (e.g., temp0.bin)
    std::string testFile = "../data/intermediate/temp509.bin";  // Update this path as needed

    // Call the function to read and print the file contents
    readAndPrintIndexFile(testFile);

    return 0;
}

/*
#include "compression.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

// Function to read and decode a binary file (docIDs and frequencies) and print the contents
bool readAndPrintIndexFile(const std::string &filename) {
    std::ifstream binFile(filename, std::ios::binary);
    if (!binFile.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return false;
    }

    // Read the whole file into a buffer
    std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(binFile)), std::istreambuf_iterator<char>());

    // Close the binary file
    binFile.close();

    // Split the buffer into docID and frequency blocks (assuming equal split between docIDs and frequencies)
    size_t block_size = buffer.size() / 2;
    if (block_size == 0) {
        std::cerr << "File " << filename << " seems to be empty or not properly formatted." << std::endl;
        return false;
    }

    std::vector<unsigned char> docIDBlock(buffer.begin(), buffer.begin() + block_size);
    std::vector<unsigned char> freqBlock(buffer.begin() + block_size, buffer.end());

    // Decode the docID and frequency blocks
    std::vector<int> docIDs = varbyteDecodeList(docIDBlock);
    std::vector<int> frequencies = varbyteDecodeList(freqBlock);

    // Check if docIDs and frequencies have the same size
    if (docIDs.size() != frequencies.size()) {
        std::cerr << "Mismatch between docID and frequency counts in file: " << filename << std::endl;
        return false;
    }

    // Print the decoded docIDs and frequencies
    std::cout << "Decoded docIDs and frequencies from file: " << filename << std::endl;
    for (size_t i = 0; i < docIDs.size(); ++i) {
        std::cout << "DocID: " << docIDs[i] << ", Frequency: " << frequencies[i] << std::endl;
    }

    return true;
}

int main() {
    // Directory containing the intermediate files
    std::string directory = "../data/intermediate/";
    std::vector<std::string> tempFiles;

    // Iterate over all temp*.bin files in the directory
    for (const auto &entry : fs::directory_iterator(directory)) {
        std::string filename = entry.path().string();
        if (filename.find("temp") != std::string::npos && filename.find(".bin") != std::string::npos) {
            tempFiles.push_back(filename);
        }
    }

    // Check if there are any files to test
    if (tempFiles.empty()) {
        std::cerr << "No temp*.bin files found in directory: " << directory << std::endl;
        return 1;
    }

    // Test and validate each temp file
    bool allFilesValid = true;
    for (const auto &file : tempFiles) {
        std::cout << "\nTesting file: " << file << std::endl;
        if (!readAndPrintIndexFile(file)) {
            allFilesValid = false;
            std::cerr << "File " << file << " is invalid." << std::endl;
        }
    }

    if (allFilesValid) {
        std::cout << "\nAll temp*.bin files are valid!" << std::endl;
    } else {
        std::cerr << "\nSome files are invalid. Please check the log above." << std::endl;
    }

    return 0;
}

*/