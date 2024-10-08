#ifndef MERGER_H
#define MERGER_H

#include <vector>
#include <string>
#include <fstream>
#include <queue>  // For priority_queue

// Declaration of the PostingEntry structure
struct PostingEntry {
    std::string term;
    int docID;
    int frequency;
    int fileIndex;

    bool operator>(const PostingEntry &other) const {
        if (term == other.term) {
            return docID > other.docID;
        }
        return term > other.term;
    }
};

// Function to load the next posting from the file and push it into the heap
bool loadNextPosting(std::ifstream &file, int fileIndex, std::priority_queue<PostingEntry, std::vector<PostingEntry>, std::greater<>> &minHeap);

// Declaration of the writeCompressedBlock function
void writeCompressedBlock(std::ofstream &binFile, const std::vector<int> &docIDs, const std::vector<int> &frequencies, 
                          std::vector<int> &lastdocid, std::vector<int> &docidsize, std::vector<int> &freqsize);

// Declaration of the extractFileNumber function
int extractFileNumber(const std::string &filename);

// Declaration of the mergeSortedFilesBatch function
void mergeSortedFilesBatch(const std::vector<std::string> &inputFiles, const std::string &outputFile);

#endif // MERGER_H
