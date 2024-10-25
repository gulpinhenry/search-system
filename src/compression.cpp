#include "compression.h"

// Function to varbyte encode a single number
void varbyteEncode(int number, std::vector<unsigned char> &encodedNumber) {
    encodedNumber.clear();
    std::vector<unsigned char> &bytes = encodedNumber;
    while (number >= 128) {
        bytes.emplace_back((number % 128) | 128);  // Set MSB
        number /= 128;
    }
    bytes.emplace_back(number);  // Final byte with MSB unset
}

// Function to varbyte encode a list of numbers
void varbyteEncodeList(const std::vector<int> &numbers, std::vector<unsigned char> &encoded) {
    encoded.clear();
    std::vector<unsigned char> encodedNumber;
    for (int number : numbers) {
        varbyteEncode(number, encodedNumber);
        encoded.insert(encoded.end(), encodedNumber.begin(), encodedNumber.end());
    }
}

// Function to varbyte decode a list of bytes back into integers
std::vector<int> varbyteDecodeList(const std::vector<unsigned char> &bytes) {
    std::vector<int> decoded;
    int number = 0;
    int shift = 0;

    for (unsigned char byte : bytes) {
        if (byte & 128) {  // Continuation bit set
            number += (byte & 127) << shift;
            shift += 7;
        } else {  // Last byte of the number
            number += byte << shift;
            decoded.emplace_back(number);
            number = 0;
            shift = 0;
        }
    }

    return std::move(decoded);
}

int varbyteDecodeNumber(const std::vector<unsigned char> &data, size_t &pos) {
    int number = 0;
    int shift = 0;
    while (pos < data.size()) {
        unsigned char byte = data[pos++];
        number |= (byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {  // Continuation bit not set
            break;
        }
        shift += 7;
    }
    return number;
}

