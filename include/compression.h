#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <vector>
#include <cstdint>

std::vector<unsigned char> varbyteEncodeList(const std::vector<int> &numbers);
std::vector<int> varbyteDecodeList(const std::vector<unsigned char> &bytes);

#endif // COMPRESSION_H
