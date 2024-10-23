// merger.cpp
#include "merge_temp_file.h"
#include "file_read_buffer.h"
#include "file_write_buffer.h"
#include "thread_pool.h"
#include "compression.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <map>
#include <cstdint>
#include <functional>
#include <filesystem>
#include <ctime>
#include <algorithm>
#include <cmath>
#include <query_processor.h>

namespace fs = std::filesystem;

// number of postings per block
const int BLOCK_SIZE = 128;
const int FILES_TO_MERGE = 8;
#define MAX_RECORDS 100000000 // max record in memory
#define THREAD_CNT 16
#define CHUNK_SIZE 40000000 // read 10MB a time
#define PARTITION_SIZE 26

std::ofstream logFile("../logs/merge_temp_file.log", std::ios::app);

void logMessage(const std::string &message)
{
    if (!logFile.is_open())
    {
        std::cerr << "Failed to open log file!" << std::endl;
        return;
    }
    std::time_t currentTime = std::time(nullptr);
    logFile << std::asctime(std::localtime(&currentTime)) << message << std::endl;
}

void mergeFiles(const std::vector<std::string> &fileNames, const std::string &outputFile)
{
    // Define the comparator as specified
    TupleComparator cmp = [](const std::tuple<std::string, int, int, float> &a,
                             const std::tuple<std::string, int, int, float> &b)
    {
        if (std::get<0>(a) == std::get<0>(b))
        {
            return std::get<1>(a) > std::get<1>(b); // Compare docIDs (smaller is better)
        }
        return std::get<0>(a) > std::get<0>(b); // Compare terms (lexicographically)
    };

    // Initialize the priority queue using the defined comparator
    TuplePQ pq(cmp);
    std::vector<FileReadBuffer> inputFiles;
    size_t fileCnt = fileNames.size();
    inputFiles.reserve(fileCnt);
    std::cout << "Merging for" << outputFile << std::endl;

    // Open all files
    for (size_t i = 0; i < fileCnt; ++i)
    {
        inputFiles.emplace_back(fileNames[i], i, MAX_RECORDS / THREAD_CNT / fileCnt, CHUNK_SIZE / THREAD_CNT / fileCnt);
        if (!inputFiles[i].isValid())
        {
            std::cerr << "Error opening file: " << fileNames[i] << std::endl;
            exit(-1);
        }
        // Read the first entry into the priority queue
        if (inputFiles[i].isValid())
        {
            auto record = inputFiles[i].getOneRecord();
            if (record != Tuple())
                pq.push(record);
        }
    }

    WriteFileBuffer output(outputFile, CHUNK_SIZE / THREAD_CNT);

    // Merge process
    while (!pq.empty())
    {
        auto smallest = pq.top();
        pq.pop();

        // Write the smallest element to the output file
        uint16_t termLength = static_cast<uint16_t>(std::get<0>(smallest).size());
        output.write(reinterpret_cast<char *>(&termLength), sizeof(termLength));
        output.write(std::get<0>(smallest).c_str(), termLength);
        output.write(reinterpret_cast<char *>(&std::get<1>(smallest)), sizeof(int));
        output.write(reinterpret_cast<char *>(&std::get<3>(smallest)), sizeof(float));

        int fileIndex = std::get<2>(smallest);
        // Read the next entry from the same file
        if (inputFiles[fileIndex].isValid())
        {
            auto record = inputFiles[fileIndex].getOneRecord();
            if (record != Tuple())
                pq.push(record);
        }
    }
}
void saveAndClearCurPostingsList(std::vector<std::pair<int, float>> &postingsList, int64_t &offset,
                                 WriteFileBuffer &indexFile, std::vector<std::pair<std::string, LexiconEntry>> &lexicon,
                                 std::string &currentTerm, std::string &term, std::vector<unsigned char> &compressedDocIDs)
{
    // Process postingsList for currentTerm
    int df = postingsList.size();
    // Sort postings by docID to ensure order
    // std::sort(postingsList.begin(), postingsList.end()); // do we really need to sort it?
    // Compute docID gaps and save termFrequencyScore
    std::vector<int> docIDGaps(postingsList.size());
    std::vector<float> termFScores(postingsList.size());
    int lastDocID = 0;
    for (size_t i = 0; i < postingsList.size(); ++i)
    {
        docIDGaps[i] = postingsList[i].first - lastDocID;
        lastDocID = postingsList[i].first;
        termFScores[i] = postingsList[i].second;
    }
    // Compress docID gaps
    varbyteEncodeList(docIDGaps, compressedDocIDs);
    // Write to index file
    int64_t offsetBefore = offset;
    indexFile.write(reinterpret_cast<char *>(compressedDocIDs.data()), compressedDocIDs.size());
    size_t termFreqScoreSize = termFScores.size() * sizeof(float);
    indexFile.write(reinterpret_cast<char *>(termFScores.data()), termFreqScoreSize);
    offset += compressedDocIDs.size() + termFreqScoreSize;
    // Update lexicon
    LexiconEntry entry;
    entry.offset = offsetBefore;
    entry.length = compressedDocIDs.size();
    entry.docFrequency = df;
    entry.blockCount = 0; // Not using blocking in this version
    lexicon.emplace_back(currentTerm, entry);
    // Reset for new term
    currentTerm = term;
    postingsList.clear();
}

std::string getIndexFileName(const std::string &startTerm)
{
    return "../data/index/index_" + startTerm + ".bin";
}

void mergeLastTempFileWithPartition(std::vector<std::string> inputFiles,
                                    const std::string &partitionTerm,
                                    const std::string &endTerm,
                                    std::vector<std::pair<std::string, LexiconEntry>> &lexicon)
{
    int numFiles = inputFiles.size();
    // Open all temp files
    std::vector<FileReadBuffer> tempFiles;
    for (int i = 0; i < numFiles; ++i)
    {
        // tempFiles[i].open("../data/intermediate/temp" + std::to_string(i) + ".bin", std::ios::binary);
        tempFiles.emplace_back(inputFiles[i], i, MAX_RECORDS / THREAD_CNT / numFiles,
                               CHUNK_SIZE / THREAD_CNT / numFiles);
        if (!tempFiles[i].isValid())
        {
            logMessage("Error opening temp file for merging.");
            return;
        }
    }

    // Output index file
    WriteFileBuffer indexFile(getIndexFileName(endTerm), CHUNK_SIZE);

    // Priority queue for merging
    TupleComparator cmp = [](const std::tuple<std::string, int, int, float> &a,
                             const std::tuple<std::string, int, int, float> &b)
    {
        if (std::get<0>(a) == std::get<0>(b))
        {
            return std::get<1>(a) > std::get<1>(b); // Compare docIDs (smaller is better)
        }
        return std::get<0>(a) > std::get<0>(b); // Compare terms (lexicographically)
    };

    // Initialize the priority queue using the defined type
    TuplePQ pq(cmp);

    // Initial read from each temp file
    for (int i = 0; i < numFiles; ++i)
    {
        if (tempFiles[i].isValid())
        {
            auto record = tempFiles[i].jumpTo(partitionTerm);
            if (endTerm != "" && std::get<0>(record) >= endTerm)
            {
                tempFiles[i].close();
            }
            else if (record != Tuple())
                pq.push(record);
        }
    }

    std::string currentTerm;
    std::vector<std::pair<int, float>> postingsList; // Stores docIDs
    int64_t offset = 0;
    std::vector<unsigned char> compressedDocIDs; // Doc id buffer

    while (!pq.empty())
    {
        auto [term, docID, fileIndex, termFreqScore] = pq.top();
        pq.pop();

        if (currentTerm.empty())
        {
            currentTerm = term;
        }

        if (term != currentTerm)
        {
            saveAndClearCurPostingsList(postingsList, offset, indexFile, lexicon, currentTerm, term, compressedDocIDs);
        }

        // Add current posting to postingsList
        postingsList.emplace_back(docID, termFreqScore);

        // Read next term-docID tuple from the same temp file
        if (tempFiles[fileIndex].isValid())
        {
            auto record = tempFiles[fileIndex].jumpTo(partitionTerm);
            if (endTerm != "" && std::get<0>(record) >= endTerm)
            {
                tempFiles[fileIndex].close();
            }
            else if (record != Tuple())
                pq.push(record);
        }
    }

    // Process postingsList for the last term
    if (!postingsList.empty())
    {
        saveAndClearCurPostingsList(postingsList, offset, indexFile, lexicon, currentTerm, currentTerm, compressedDocIDs);
    }
    std::cout << "Total write for" << getIndexFileName(endTerm) << " is " << offset << std::endl;
    // don't need to close files
    logMessage("Merging completed.");
}

void mergeBinaryFiles(const std::vector<std::string> &filenames,
                      std::vector<std::vector<std::pair<std::string, LexiconEntry>>> &lexicons,
                      const std::string &outputFilename,
                      std::vector<std::pair<std::string, LexiconEntry>> &outputLexicon)
{
    WriteFileBuffer outputFile(outputFilename, CHUNK_SIZE);
    std::cout << "Start merging binary file"
              << filenames.size()
              << "<-should be equal->"
              << lexicons.size() << std::endl;
    int64_t offset = 0;
#define MERGE_BUFFER_LEN 500000
    char buffer[MERGE_BUFFER_LEN];
    for (int i = 0; i < lexicons.size(); i++)
    {
        std::string filename = filenames[i];
        std::cout << "Merging  " << filename << " in one." << std::endl;
        std::ifstream inputFile(filename, std::ios::binary);
        int64_t totalRead = 0, fileTotalLen = 0;
        if (!inputFile)
        {
            std::cerr << "Error: Could not open input file " << filename << std::endl;
            continue; // Skip this file and continue with the next
        }
        std::cout << "file open successfully" << std::endl;

        for (auto &pair : lexicons[i])
        {
            std::string term = pair.first;
            auto lexicon = pair.second;
            auto totalLen = lexicon.length + lexicon.docFrequency * sizeof(float);
            lexicon.offset = offset;
            offset += totalLen;
            fileTotalLen += totalLen;
            outputLexicon.emplace_back(term, lexicon);
        }
        for (size_t bytesRead = 0; bytesRead < fileTotalLen;)
        {
            size_t toRead = std::min((uint64_t)MERGE_BUFFER_LEN, fileTotalLen - bytesRead);
            inputFile.read(buffer, toRead);
            size_t actuallyRead = inputFile.gcount(); // Get the actual number of bytes read
            totalRead += actuallyRead;
            outputFile.write(buffer, actuallyRead);
            bytesRead += actuallyRead;
        }
        inputFile.close();
    }
}

void mergeLastTempFile(std::vector<std::string> inputFiles,
                       std::vector<std::pair<std::string, LexiconEntry>> &lexicon,
                       std::vector<std::string> termsVec,
                       ThreadPool &threadPool)
{
    termsVec.push_back(""); // add the end term
    std::vector<std::vector<std::pair<std::string, LexiconEntry>>> orderedLexicons(termsVec.size(),
                                                                                   std::vector<std::pair<std::string, LexiconEntry>>());
    for (int i = 0; i < termsVec.size(); i++)
    {
        std::cout << "Term: " + termsVec[i] << std::endl;
        auto start = i == 0 ? "" : termsVec[i - 1];
        auto end = termsVec[i];
        auto task = [inputFiles, start, end, i, &orderedLexicons]
        {
            mergeLastTempFileWithPartition(inputFiles, start, end, orderedLexicons[i]);
        };
        threadPool.enqueue(task);
    }
    threadPool.waitAll();
    uint32_t cnt = 0;
    lexicon.clear();
    int64_t offset = 0;
    std::vector<std::string> fileNames;
    for (int i = 0; i < termsVec.size(); i++) // ignore the last end term
    {
        fileNames.push_back(getIndexFileName(termsVec[i]));
    }
    mergeBinaryFiles(fileNames, orderedLexicons, "../data/index.bin", lexicon);
}

// Function to write the lexicon to a file
void writeLexiconToFile(const std::vector<std::pair<std::string, LexiconEntry>> &lexicon)
{
    WriteFileBuffer lexiconFile("../data/lexicon.bin", CHUNK_SIZE);
    uint32_t cnt = 0;
    for (const auto &[term, entry] : lexicon)
    {
        if (cnt++ % 10000 == 0)
        {
            std::cout << "Lexicon: " << term << entry.length << std::endl;
        }
        // Write term length and term
        uint16_t termLength = static_cast<uint16_t>(term.length());
        lexiconFile.write(reinterpret_cast<const char *>(&termLength), sizeof(termLength));
        lexiconFile.write(term.c_str(), termLength);

        // Write LexiconEntry data
        lexiconFile.write(reinterpret_cast<const char *>(&entry.offset), sizeof(entry.offset));
        lexiconFile.write(reinterpret_cast<const char *>(&entry.length), sizeof(entry.length));
        lexiconFile.write(reinterpret_cast<const char *>(&entry.docFrequency), sizeof(entry.docFrequency));

        // Since we're not using blocking, set blockCount to 0
        lexiconFile.write(reinterpret_cast<const char *>(&entry.blockCount), sizeof(entry.blockCount));
    }

    logMessage("Lexicon written to file.");
}

#include <chrono>

int main()
{
    auto startTime = std::chrono::high_resolution_clock::now();
    std::vector<std::string> filesToMerge;
    // Read the number of temp files generated
    int numTempFiles = 0;
    while (true)
    {
        std::string tempFileName = "../data/intermediate/temp" + std::to_string(numTempFiles) + ".bin";
        if (!fs::exists(tempFileName))
        {
            break;
        }
        filesToMerge.push_back(tempFileName);
        numTempFiles++;
    }

    if (numTempFiles == 0)
    {
        std::cerr << "No temp files found for merging." << std::endl;
        return 1;
    }

    ThreadPool pool(THREAD_CNT);
    int fileCounter = 0;
    while (filesToMerge.size() > 1)
    {
        int outputCnt = std::ceil(static_cast<float>(filesToMerge.size()) / FILES_TO_MERGE);
        if (outputCnt > 1)
        {
            std::vector<std::string> outputFilenames(outputCnt);

            for (int i = 0; i < outputCnt; i++)
            {
                // Generate the output filename
                std::string outputFileName = "../data/intermediate/merging" + std::to_string(fileCounter++) + ".bin";
                outputFilenames[i] = outputFileName;

                // Gather the files for this batch
                std::vector<std::string> batchFiles;
                for (int j = 0; j < FILES_TO_MERGE && (i * FILES_TO_MERGE + j) < filesToMerge.size(); j++)
                {
                    batchFiles.push_back(filesToMerge[i * FILES_TO_MERGE + j]);
                }
                // Log the files being merged and the output filename
                std::cout << "Enqueuing merge task for files: size: " << batchFiles.size();
                std::cout << "-> Output file: " << outputFileName << std::endl;
                // Enqueue the merge task to the thread pool
                pool.enqueue([batchFiles, outputFileName]
                             { mergeFiles(batchFiles, outputFileName); });
            }

            pool.waitAll();

            // Update the files to merge with the newly created output files
            filesToMerge = std::move(outputFilenames);
        }
        else
        {
            std::vector<std::pair<std::string, LexiconEntry>> lexicon;
            std::cout << "Merging into one last file" << std::endl;
            std::vector<std::string> termsVec;
            for (auto c = 'a'; c <= 'z'; c++)
            {
                termsVec.push_back(std::string(1, c));
            }

            // Merge temp files to create the inverted index and lexicon
            mergeLastTempFile(filesToMerge, lexicon, termsVec, pool);

            // Write the lexicon to file
            writeLexiconToFile(lexicon);
            break;
        }
    }

    logMessage("Merging process completed.");
    auto endTime = std::chrono::high_resolution_clock::now();

    std::cout << "time passed: " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << std::endl;

    return 0;
}
