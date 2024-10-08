#include "query_processor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

// Constants for BM25
const double k1 = 1.2;
const double b = 0.75;

// Constructor
QueryProcessor::QueryProcessor(const std::string &indexFilename, const std::string &lexiconFilename, const std::string &pageTableFilename)
    : invertedIndex(indexFilename, lexiconFilename), totalDocs(0), avgDocLength(0.0) {
    // Load the page table
    loadPageTable(pageTableFilename);

    // Calculate total number of documents and average document length
    totalDocs = pageTable.size();
    int totalLength = 0;
    for (const auto &[docID, docName] : pageTable) {
        // Assuming document length is 100 for simplicity; adjust as needed
        docLengths[docID] = 100;
        totalLength += 100;
    }
    avgDocLength = static_cast<double>(totalLength) / totalDocs;
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

// Compute BM25 score for a document
double QueryProcessor::computeBM25(int docID, const std::vector<std::string> &terms, const std::unordered_map<std::string, int> &termFrequencies) {
    double score = 0.0;
    int docLength = docLengths[docID];
    for (const auto &term : terms) {
        int df = invertedIndex.getDocFrequency(term);
        int tf = termFrequencies.at(term);

        double idf = log((totalDocs - df + 0.5) / (df + 0.5) + 1);
        double numerator = tf * (k1 + 1);
        double denominator = tf + k1 * (1 - b + b * docLength / avgDocLength);
        score += idf * (numerator / denominator);
    }
    return score;
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

    // Adjusted code to collect term frequencies per document
    std::unordered_map<int, std::unordered_map<std::string, int>> docTermFreqs;  // docID -> {term -> frequency}

    if (conjunctive) {
        // Conjunctive query: intersect the postings
        size_t numTerms = termPostings.size();
        std::vector<size_t> indices(numTerms, 0);

        while (true) {
            int currentDocID = -1;
            bool allMatch = true;

            // Find the maximum docID among current postings
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
                for (size_t i = 0; i < numTerms; ++i) {
                    docTermFreqs[currentDocID][validTerms[i]] = termPostings[i][indices[i]].frequency;
                    indices[i]++;
                }
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
        for (size_t termIdx = 0; termIdx < termPostings.size(); ++termIdx) {
            const auto &postings = termPostings[termIdx];
            const auto &term = validTerms[termIdx];
            for (const auto &posting : postings) {
                docTermFreqs[posting.docID][term] = posting.frequency;
            }
        }
    }

    // Rank documents using BM25
    std::vector<std::pair<int, double>> rankedDocs;
    for (const auto &[docID, termFreqs] : docTermFreqs) {
        double score = computeBM25(docID, validTerms, termFreqs);
        rankedDocs.emplace_back(docID, score);
    }

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