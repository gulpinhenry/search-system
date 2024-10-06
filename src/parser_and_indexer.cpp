#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "parser_and_indexer.h"

using namespace std;

vector<string> Tokenize(const string &text) {
    stringstream ss(text);
    string word;
    vector<string> tokens;

    while (ss >> word) {
        tokens.push_back(word);
    }
    return tokens;
}

int main() {
    ifstream dataFile("../data/ms_marco_passages.tsv");
    ofstream outputFile("../data/intermediate/sorted_postings.txt");

    if (!dataFile.is_open() || !outputFile.is_open()) {
        cerr << "Error opening data file or output file" << endl;
        return 1;
    }

    unordered_map<string, vector<pair<int, int>>> postings;
    int docID = 0;
    string line;

    while (getline(dataFile, line)) {
        docID++;
        vector<string> terms = Tokenize(line);
        unordered_map<string, int> termFrequencies;

        for (const string &term : terms) {
            termFrequencies[term]++;
        }

        for (const auto &entry : termFrequencies) {
            postings[entry.first].push_back({docID, entry.second});
        }
    }

    for (const auto &entry : postings) {
        outputFile << entry.first << " ";
        for (const auto &posting : entry.second) {
            outputFile << posting.first << ":" << posting.second << " ";
        }
        outputFile << endl;
    }

    dataFile.close();
    outputFile.close();

    cout << "Parsing and indexing completed." << endl;
    return 0;
}
