#ifndef FILE_WRITE_BUFFER
#define FILE_WRITE_BUFFER
#include <string>
#include <fstream>
#include <iostream>

class WriteFileBuffer {
public:
    WriteFileBuffer(const std::string& filename, std::size_t chunkSize)
        : _chunkSize(chunkSize), _buffer() {
        // Open the file for output
        _outputStream.open(filename, std::ios::binary);
        if (!_outputStream.is_open()) {
            throw std::runtime_error("Error opening output file: " + filename);
        }
    }

    ~WriteFileBuffer() {
        // Ensure any remaining data is written to the output stream on destruction
        flush();
        _outputStream.close(); // Close the file stream
    }

    // Method to write data similar to ofstream::write
    void write(const char* data, std::size_t size) {
        if (size == 0) return; // Nothing to write

        // If adding data exceeds the chunk size, flush the current buffer
        if (_buffer.size() + size > _chunkSize) {
            flush();
        }

        // Append the new data to the buffer
        _buffer.append(data, size);
    }

    void flush() {
        if (_buffer.empty()) return; // No data to write

        // Write the buffered data to the output stream
        _outputStream.write(_buffer.data(), _buffer.size());

        // Clear the buffer after writing
        _buffer.clear();
    }

private:
    std::ofstream _outputStream; // Output file stream
    std::size_t _chunkSize;      // Maximum size of the buffer before writing
    std::string _buffer;          // Buffer to hold data in memory
};

#endif
