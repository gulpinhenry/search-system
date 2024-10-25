#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <vector>
#include <cstdint>
#include <cstddef>

void varbyteEncode(int number, std::vector<unsigned char> &encodedNumber);
void varbyteEncodeList(const std::vector<int> &numbers, std::vector<unsigned char> &encoded);
std::vector<int> varbyteDecodeList(const std::vector<unsigned char> &bytes);
int varbyteDecodeNumber(const std::vector<unsigned char> &data, size_t &pos);

#endif // COMPRESSION_H
