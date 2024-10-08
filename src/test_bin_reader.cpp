#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

// Function to read and print term-docID pairs from temp files
void readAndPrintTempFiles(const std::string &directory) {
    int fileCounter = 0;
    while (true) {
        std::string tempFileName = directory + "temp" + std::to_string(fileCounter++) + ".bin";
        if (!fs::exists(tempFileName)) {
            break;
        }
        std::ifstream tempFile(tempFileName, std::ios::binary);
        if (!tempFile.is_open()) {
            std::cerr << "Error opening temp file: " << tempFileName << std::endl;
            continue;
        }
        std::cout << "Contents of " << tempFileName << ":\n";
        while (tempFile.peek() != EOF) {
            uint16_t termLength;
            tempFile.read(reinterpret_cast<char*>(&termLength), sizeof(termLength));
            std::string term(termLength, ' ');
            tempFile.read(&term[0], termLength);
            int docID;
            tempFile.read(reinterpret_cast<char*>(&docID), sizeof(docID));
            std::cout << "Term: " << term << ", DocID: " << docID << std::endl;
        }
        tempFile.close();
    }
}

int main() {
    // Directory where the temp*.bin files are stored
    std::string directory = "../data/intermediate/";

    // Read and print term-docID pairs from temp files
    readAndPrintTempFiles(directory);

    return 0;
}
