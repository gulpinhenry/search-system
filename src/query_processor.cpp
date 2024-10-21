// query_processor.cpp
#include "query_processor.h"
#include "compression.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <queue>
#include <tuple>
#include <cmath>

// Constants for BM25
const double k1 = 1.5;
const double b = 0.75;
const int64_t documentLen = 8841823;

// --- InvertedListPointer Implementation ---

InvertedListPointer::InvertedListPointer(std::ifstream *indexFile, const LexiconEntry &lexEntry)
    : indexFile(indexFile), lexEntry(lexEntry), currentDocID(-1), valid(true),
      lastDocID(0), bufferPos(0), termFreqScoreIndex(-1) {
    // Read compressed data from index file
    indexFile->seekg(lexEntry.offset, std::ios::beg);
    compressedData.resize(lexEntry.length);
    indexFile->read(reinterpret_cast<char*>(compressedData.data()), lexEntry.length);
    termFreqScore.resize(lexEntry.length);
    indexFile->read(reinterpret_cast<char*>(termFreqScore.data()), lexEntry.length * sizeof(float));

}

bool InvertedListPointer::next() {
    if (!valid) return false;

    if (bufferPos >= compressedData.size()) {
        valid = false;
        return false;
    }
    termFreqScoreIndex++;
    if (termFreqScoreIndex >= termFreqScore.size()) {
        valid = false;
        return false;
    }

    // Decompress next docID gap
    int gap = varbyteDecodeNumber(compressedData, bufferPos);
    lastDocID += gap;
    currentDocID = lastDocID;

    return true;
}

bool InvertedListPointer::nextGEQ(int docID) {
    while (valid && currentDocID < docID) {
        if (!next()) {
            return false;
        }
    }
    return valid;
}

int InvertedListPointer::getDocID() const {
    return currentDocID;
}

float InvertedListPointer::getTFS() const {
    return termFreqScore[termFreqScoreIndex];
}

float InvertedListPointer::getIDF() const {
    return lexEntry.IDF;
}

int InvertedListPointer::getTF() const {
    // Since TF is not stored, we might return a default value
    return 1;
}

int InvertedIndex::getDocFrequency(const std::string &term) {
    auto it = lexicon.find(term);
    if (it != lexicon.end()) {
        return it->second.docFrequency;
    } else {
        return 0;
    }
}


bool InvertedListPointer::isValid() const {
    return valid;
}

void InvertedListPointer::close() {
    valid = false;
}

// --- InvertedIndex Implementation ---

InvertedIndex::InvertedIndex(const std::string &indexFilename, const std::string &lexiconFilename) {
    // Load the lexicon from lexiconFilename
    loadLexicon(lexiconFilename);

    // Open index file
    indexFile.open(indexFilename, std::ios::binary);
    if (!indexFile.is_open()) {
        std::cerr << "Error opening index file: " << indexFilename << std::endl;
        return;
    }
}

void InvertedIndex::loadLexicon(const std::string &lexiconFilename) {
    std::ifstream lexiconFile(lexiconFilename, std::ios::binary);
    if (!lexiconFile.is_open()) {
        std::cerr << "Error opening lexicon file: " << lexiconFilename << std::endl;
        return;
    }

    while (lexiconFile.peek() != EOF) {
        uint16_t termLength;
        lexiconFile.read(reinterpret_cast<char*>(&termLength), sizeof(termLength));
        if (!lexiconFile) break; // EOF or error

        std::vector<char> termBuffer(termLength);
        lexiconFile.read(termBuffer.data(), termLength);
        if (!lexiconFile) break; // EOF or error

        std::string term(termBuffer.begin(), termBuffer.end());

        LexiconEntry entry;
        lexiconFile.read(reinterpret_cast<char*>(&entry.offset), sizeof(entry.offset));
        lexiconFile.read(reinterpret_cast<char*>(&entry.length), sizeof(entry.length));
        lexiconFile.read(reinterpret_cast<char*>(&entry.docFrequency), sizeof(entry.docFrequency));
        lexiconFile.read(reinterpret_cast<char*>(&entry.blockCount), sizeof(entry.blockCount));

        entry.IDF = log((documentLen - entry.docFrequency + 0.5) / (entry.docFrequency + 0.5));

        // If using blocking, read block metadata
        if (entry.blockCount > 0) {
            // Read blockMaxDocIDs
            entry.blockMaxDocIDs.resize(entry.blockCount);
            for (int i = 0; i < entry.blockCount; ++i) {
                lexiconFile.read(reinterpret_cast<char*>(&entry.blockMaxDocIDs[i]), sizeof(entry.blockMaxDocIDs[i]));
            }

            // Read blockOffsets
            entry.blockOffsets.resize(entry.blockCount);
            for (int i = 0; i < entry.blockCount; ++i) {
                lexiconFile.read(reinterpret_cast<char*>(&entry.blockOffsets[i]), sizeof(entry.blockOffsets[i]));
            }
        }

        lexicon[term] = entry;
    }
    lexiconFile.close();
}

bool InvertedIndex::openList(const std::string &term) {
    return lexicon.find(term) != lexicon.end();
}

InvertedListPointer InvertedIndex::getListPointer(const std::string &term) {
    return InvertedListPointer(&indexFile, lexicon[term]);
}

void InvertedIndex::closeList(const std::string &term) {
    // No action needed as we're not keeping any state per term
}

// --- QueryProcessor Implementation ---

// Constructor
QueryProcessor::QueryProcessor(const std::string &indexFilename, const std::string &lexiconFilename, const std::string &pageTableFilename, const std::string &docLengthsFilename)
    : invertedIndex(indexFilename, lexiconFilename) {
    // Load the page table
    loadPageTable(pageTableFilename);

    // Load document lengths
    loadDocumentLengths(docLengthsFilename);

    // Total number of documents
    totalDocs = docLengths.size();
    // After loading document lengths
    std::cout << "Total Documents: " << totalDocs << std::endl;

    // Compute average document length
    double totalDocLength = 0.0;
    for (const auto& [docID, length] : docLengths) {
        totalDocLength += length;
    }
    avgDocLength = totalDocLength / totalDocs;
    std::cout << "Average Document Length: " << avgDocLength << std::endl;
}

// Parse the query into terms
std::vector<std::string> QueryProcessor::parseQuery(const std::string &query) {
    std::vector<std::string> terms;
    std::istringstream iss(query);
    std::string term;
    while (iss >> term) {
        // Normalize term: lowercase, remove punctuation
        term.erase(std::remove_if(term.begin(), term.end(), ::ispunct), term.end());
        std::transform(term.begin(), term.end(), term.begin(), ::tolower);
        if (!term.empty()) {
            terms.push_back(term);
        }
    }
    return terms;
}

// Load the page table from file
void QueryProcessor::loadPageTable(const std::string &pageTableFilename) {
    std::ifstream pageTableFile(pageTableFilename, std::ios::binary);
    if (!pageTableFile.is_open()) {
        std::cerr << "Error opening page table file: " << pageTableFilename << std::endl;
        return;
    }

    while (pageTableFile.peek() != EOF) {
        int docID;
        pageTableFile.read(reinterpret_cast<char*>(&docID), sizeof(docID));

        uint16_t nameLength;
        pageTableFile.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));

        std::string docName(nameLength, ' ');
        pageTableFile.read(&docName[0], nameLength);

        pageTable[docID] = docName;
    }

    pageTableFile.close();
}

// Load document lengths from file
void QueryProcessor::loadDocumentLengths(const std::string &docLengthsFilename) {
    std::ifstream docLengthsFile(docLengthsFilename, std::ios::binary);
    if (!docLengthsFile.is_open()) {
        std::cerr << "Error opening document lengths file: " << docLengthsFilename << std::endl;
        return;
    }

    while (docLengthsFile.peek() != EOF) {
        int docID;
        int docLength;
        docLengthsFile.read(reinterpret_cast<char*>(&docID), sizeof(docID));
        docLengthsFile.read(reinterpret_cast<char*>(&docLength), sizeof(docLength));
        docLengths[docID] = docLength;
    }
    // After loading document lengths
    std::cout << "Number of Document Lengths Loaded: " << docLengths.size() << std::endl;

    for (const auto& [docID, length] : docLengths) {
        if (length <= 0) {
            std::cout << "Invalid document length for DocID " << docID << ": " << length << std::endl;
        }
    }

    docLengthsFile.close();
}
void QueryProcessor::processQuery(const std::string &query, bool conjunctive) {
    auto terms = parseQuery(query);

    if (terms.empty()) {
        std::cout << "No terms found in query." << std::endl;
        return;
    }

    // Open inverted lists for each term and store term-pointer pairs
    std::vector<std::pair<std::string, InvertedListPointer>> termPointers;
    for (const auto &term : terms) {
        if (!invertedIndex.openList(term)) {
            std::cout << "Term not found: " << term << std::endl;
            continue;
        }
        termPointers.emplace_back(term, invertedIndex.getListPointer(term));
    }

    if (termPointers.empty()) {
        std::cout << "No valid terms found in query." << std::endl;
        return;
    }

    // DAAT Processing
    std::unordered_map<int, double> docScores;  // docID -> aggregated score

    if (conjunctive) {
        // Initialize docIDs for each list
        std::vector<int> docIDs;
        for (auto &tp : termPointers) {
            auto &ptr = tp.second;
            if (!ptr.isValid()) {  // Check if list is empty
                ptr.close();
                return;  // One of the lists is empty, no results
            }
            if (!ptr.next()) {  // Move to first element
                ptr.close();
                return;  // Empty list
            }
            docIDs.push_back(ptr.getDocID());
        }

        while (true) {
            int maxDocID = *std::max_element(docIDs.begin(), docIDs.end());
            bool allMatch = true;

            // Advance pointers where docID < maxDocID
            for (size_t i = 0; i < termPointers.size(); ++i) {
                auto &ptr = termPointers[i].second;
                while (docIDs[i] < maxDocID) {
                    if (!ptr.nextGEQ(maxDocID)) {
                        allMatch = false;
                        break;  // Reached end of list
                    }
                    docIDs[i] = ptr.getDocID();
                }
                if (docIDs[i] != maxDocID) {
                    allMatch = false;
                }
            }

            if (!allMatch) {
                // Check if any list has reached the end
                bool anyEnd = false;
                for (auto &tp : termPointers) {
                    if (!tp.second.isValid()) {
                        anyEnd = true;
                        break;
                    }
                }
                if (anyEnd) break;
                continue;
            }
            // allMatch equal to true
            // All pointers are at the same docID
            int docID = maxDocID;
            double totalScore = 0.0;
            for (auto &tp : termPointers) {
                auto &ptr = tp.second;
                const std::string &term = tp.first;

                // Compute BM25 score
                // int tf = ptr.getTF(); // Assuming TF = 1
                // int docLength = docLengths[docID];
                // int df = invertedIndex.getDocFrequency(term);
                // double idf = std::log((totalDocs - df + 0.5) / (df + 0.5));
                // double K = k1 * ((1 - b) + b * (static_cast<double>(docLength) / avgDocLength));
                // double bm25Score = idf * ((k1 + 1) * tf) / (K + tf);
                float bm25Score = ptr.getIDF() * ptr.getTFS();
                totalScore += bm25Score;

                ptr.next();  // Advance pointer for next iteration
            }
            docScores[docID] = totalScore;

            // Update docIDs
            bool validPointers = true;
            for (size_t i = 0; i < termPointers.size(); ++i) {
                auto &ptr = termPointers[i].second;
                if (ptr.isValid()) {
                    docIDs[i] = ptr.getDocID();
                } else {
                    // One of the lists has reached the end
                    validPointers = false;
                    break;
                }
            }
            if (!validPointers) break;
        }
    } else {
        // Disjunctive query processing using a min-heap
        auto cmp = [](std::pair<InvertedListPointer*, std::string> a, std::pair<InvertedListPointer*, std::string> b) {
            return a.first->getDocID() > b.first->getDocID();
        };
        std::priority_queue<
            std::pair<InvertedListPointer*, std::string>,
            std::vector<std::pair<InvertedListPointer*, std::string>>,
            decltype(cmp)> pq(cmp);

        // Initialize heap with the first posting from each term
        for (auto &tp : termPointers) {
            auto &ptr = tp.second;
            const std::string &term = tp.first;
            if (ptr.isValid() && ptr.next()) {
                pq.push({&ptr, term});
            }
        }

        while (!pq.empty()) {
            auto [ptr, term] = pq.top();
            pq.pop();

            int docID = ptr->getDocID();

            // Compute BM25 score
            // int tf = ptr->getTF(); // Assuming TF = 1
            // int docLength = docLengths[docID];
            // int df = invertedIndex.getDocFrequency(term);
            // double idf = std::log((totalDocs - df + 0.5) / (df + 0.5));
            // double K = k1 * ((1 - b) + b * (static_cast<double>(docLength) / avgDocLength));
            // double bm25Score = idf * ((k1 + 1) * tf) / (K + tf);
            float bm25Score = ptr->getIDF() * ptr->getTFS();

            docScores[docID] += bm25Score;

            // Advance the pointer and re-add to the heap if valid
            if (ptr->next()) {
                pq.push({ptr, term});
            }
        }
    }

    // Close all inverted lists
    for (auto &tp : termPointers) {
        tp.second.close();
    }

    if (docScores.empty()) {
        std::cout << "No documents matched the query." << std::endl;
        return;
    }

    // Rank documents by aggregated score
    std::vector<std::pair<int, double>> rankedDocs(docScores.begin(), docScores.end());

    // Sort by score in descending order
    std::sort(rankedDocs.begin(), rankedDocs.end(), [](const auto &a, const auto &b) {
        return a.second > b.second;
    });

    // Display top 10 results
    int resultsCount = std::min(10, static_cast<int>(rankedDocs.size()));
    for (int i = 0; i < resultsCount; ++i) {
        int docID = rankedDocs[i].first;
        double score = rankedDocs[i].second;
        std::string docName = pageTable[docID];
        std::cout << i + 1 << ". DocID: " << docID << ", DocName: " << docName << ", Score: " << score << std::endl;
    }
}
// --- Main Function ---

int main() {
    QueryProcessor qp("../data/index.bin", "../data/lexicon.bin", "../data/page_table.bin", "../data/doc_lengths.bin");

    std::string query;
    std::string mode;

    std::cout << "Welcome to the Query Processor!" << std::endl;
    while (true) {
        std::cout << "\nEnter your query (or type 'exit' to quit): ";
        std::getline(std::cin, query);

        if (query == "exit") {
            break;
        }

        std::cout << "Choose mode (AND/OR): ";
        std::getline(std::cin, mode);

        bool conjunctive = (mode == "AND" || mode == "and");

        qp.processQuery(query, conjunctive);
    }

    return 0;
}
