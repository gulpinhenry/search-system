#include "compression.h"

// Function to varbyte encode a single number
std::vector<unsigned char> varbyteEncode(int number) {
    std::vector<unsigned char> bytes;
    while (number >= 128) {
        bytes.push_back((number % 128) | 128);  // Set MSB
        number /= 128;
    }
    bytes.push_back(number);  // Final byte with MSB unset
    return bytes;
}

// Function to varbyte encode a list of numbers
std::vector<unsigned char> varbyteEncodeList(const std::vector<int> &numbers) {
    std::vector<unsigned char> encoded;
    for (int number : numbers) {
        std::vector<unsigned char> encodedNumber = varbyteEncode(number);
        encoded.insert(encoded.end(), encodedNumber.begin(), encodedNumber.end());
    }
    return encoded;
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
            decoded.push_back(number);
            number = 0;
            shift = 0;
        }
    }

    return decoded;
}
