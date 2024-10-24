// merger.h
#ifndef MERGER_H
#define MERGER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include "lexicon_entry.h"

// Function prototypes
void mergeTempFiles(int numFiles);

void logMessage(const std::string &message);

#endif  // MERGER_H
