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
#include <chrono>

void QueryProcessor::processQuery(const std::string &query, bool conjunctive) {
    auto startTime = std::chrono::high_resolution_clock::now();
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
        std::cout << "Starting docID size: " << docIDs.size() << std::endl;
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
    auto endTime = std::chrono::high_resolution_clock::now();
    std::cout << "time passed: " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << std::endl;

}
// --- Main Function ---
#include <chrono>
int main() {
    QueryProcessor qp("../data/index.bin", "../data/lexicon.bin", "../data/page_table.bin", "../data/doc_lengths.bin");

    std::string query;
    std::string mode;

    std::cout << "Welcome to the Query Processor!" << std::endl;
    while (true) {
        std::cout << "\nEnter your query (or type 'exit' to quit): " << std::flush;
        std::getline(std::cin, query);
        
        if (query == "exit") {
            break;
        }

        std::cout << "Choose mode (AND/OR): " << std::flush;
        std::getline(std::cin, mode);

        bool conjunctive = (mode == "AND" || mode == "and");
        auto startTime = std::chrono::high_resolution_clock::now();
        qp.processQuery(query, conjunctive);
        auto endTime = std::chrono::high_resolution_clock::now();
        std::cout << "time passed: " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << std::endl;
    }

    return 0;
}
