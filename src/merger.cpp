#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include "merger.h"

using namespace std;

// Simple function to merge intermediate postings
int main() {
    ifstream inputFile("../data/intermediate/sorted_postings.txt");
    ofstream indexFile("../data/final_index/inverted_index.dat");
    ofstream lexiconFile("../data/final_index/lexicon.dat");

    if (!inputFile.is_open() || !indexFile.is_open() || !lexiconFile.is_open()) {
        cerr << "Error opening files for merging" << endl;
        return 1;
    }

    string line;
    long offset = 0;

    while (getline(inputFile, line)) {
        stringstream ss(line);
        string term;
        ss >> term;

        lexiconFile << term << " " << offset << endl;

        indexFile << line << endl;
        offset = indexFile.tellp();
    }

    inputFile.close();
    indexFile.close();
    lexiconFile.close();

    cout << "Merging completed." << endl;
    return 0;
}
