#include "query_processor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <queue>
#include <tuple>
#include <unordered_map>

// Constructor
QueryProcessor::QueryProcessor(const std::string &indexFilename, const std::string &lexiconFilename, const std::string &pageTableFilename)
    : invertedIndex(indexFilename, lexiconFilename) {
    // Load the page table
    loadPageTable(pageTableFilename);

    // Total number of documents
    totalDocs = pageTable.size();

    // Load document lengths if necessary (not needed if BM25 scores are precomputed)
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

// Process a query and return the top 10 results
void QueryProcessor::processQuery(const std::string &query, bool conjunctive) {
    auto terms = parseQuery(query);

    if (terms.empty()) {
        std::cout << "No terms found in query." << std::endl;
        return;
    }

    // Open inverted lists for each term
    std::vector<std::vector<Posting>> termPostings;
    std::vector<std::string> validTerms;
    for (const auto &term : terms) {
        if (!invertedIndex.openList(term)) {
            std::cout << "Term not found: " << term << std::endl;
            continue;
        }

        // Collect postings for the term
        Posting posting;
        std::vector<Posting> postings;
        while (invertedIndex.nextPosting(posting)) {
            postings.push_back(posting);
        }
        termPostings.push_back(std::move(postings));
        validTerms.push_back(term);
        invertedIndex.closeList();
    }

    if (validTerms.empty()) {
        std::cout << "No valid terms found in query." << std::endl;
        return;
    }

    // DAAT Processing
    std::unordered_map<int, double> docScores;  // docID -> aggregated score

    if (conjunctive) {
        // Conjunctive query: intersect the postings
        size_t numTerms = termPostings.size();
        std::vector<size_t> indices(numTerms, 0);

        while (true) {
            int currentDocID = -1;
            bool allMatch = true;

            // Find the docID with the smallest value among current postings
            for (size_t i = 0; i < numTerms; ++i) {
                if (indices[i] >= termPostings[i].size()) {
                    allMatch = false;
                    break;
                }
                int docID = termPostings[i][indices[i]].docID;
                if (currentDocID == -1 || docID > currentDocID) {
                    currentDocID = docID;
                }
            }

            if (!allMatch) {
                break;
            }

            // Check if all postings have this docID
            for (size_t i = 0; i < numTerms; ++i) {
                while (indices[i] < termPostings[i].size() && termPostings[i][indices[i]].docID < currentDocID) {
                    indices[i]++;
                }
                if (indices[i] >= termPostings[i].size() || termPostings[i][indices[i]].docID != currentDocID) {
                    allMatch = false;
                    break;
                }
            }

            if (allMatch) {
                // All terms have this docID
                double totalScore = 0.0;
                for (size_t i = 0; i < numTerms; ++i) {
                    totalScore += termPostings[i][indices[i]].bm25Score;
                    indices[i]++;
                }
                docScores[currentDocID] = totalScore;
            } else {
                // Increment indices where docID is less than currentDocID
                for (size_t i = 0; i < numTerms; ++i) {
                    if (indices[i] < termPostings[i].size() && termPostings[i][indices[i]].docID < currentDocID) {
                        indices[i]++;
                    }
                }
            }
        }
    } else {
        // Disjunctive query: union of postings
        // Use a min-heap to merge postings
        auto cmp = [](const std::tuple<int, size_t, size_t> &a, const std::tuple<int, size_t, size_t> &b) {
            return std::get<0>(a) > std::get<0>(b);  // Compare docIDs
        };
        std::priority_queue<
            std::tuple<int, size_t, size_t>,
            std::vector<std::tuple<int, size_t, size_t>>,
            decltype(cmp)
        > pq(cmp);

        // Initialize heap with the first posting from each term
        for (size_t i = 0; i < termPostings.size(); ++i) {
            if (!termPostings[i].empty()) {
                pq.emplace(termPostings[i][0].docID, i, 0);  // (docID, termIndex, postingIndex)
            }
        }

        while (!pq.empty()) {
            auto [docID, termIdx, postingIdx] = pq.top();
            pq.pop();

            double score = termPostings[termIdx][postingIdx].bm25Score;
            docScores[docID] += score;

            // Move to next posting in the same term
            if (postingIdx + 1 < termPostings[termIdx].size()) {
                pq.emplace(termPostings[termIdx][postingIdx + 1].docID, termIdx, postingIdx + 1);
            }
        }
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

int main() {
    QueryProcessor qp("../data/index.bin", "../data/lexicon.bin", "../data/page_table.bin");

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
