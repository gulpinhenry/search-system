#include "compression.h"
#include <vector>
#include <iostream>

using namespace std;

// TODO: actually implement this
vector<int> VarbyteEncode(const vector<int> &input) {
    vector<int> encoded;
    
    for (int number : input) {
        while (true) {
            int byte = number & 0x7F;  // Extract the 7 least significant bits
            number >>= 7;

            // If there are more bits to come, set the continuation bit (MSB)
            if (number > 0) {
                byte |= 0x80; // Set MSB to 1
            }

            encoded.push_back(byte);

            // If we've fully encoded this number, break out of the loop
            if ((byte & 0x80) == 0) {
                break;
            }
        }
    }

    return encoded;
}

vector<int> VarbyteDecode(const vector<int> &encoded) {
    vector<int> decoded;
    int currentNumber = 0;
    int shiftAmount = 0;

    for (int byte : encoded) {
        currentNumber |= (byte & 0x7F) << shiftAmount;  // Take the 7 least significant bits

        // If MSB is not set, we are at the last byte of this number
        if ((byte & 0x80) == 0) {
            decoded.push_back(currentNumber);
            currentNumber = 0;
            shiftAmount = 0;
        } else {
            // MSB is set, continue with the next byte
            shiftAmount += 7;
        }
    }

    return decoded;
}
