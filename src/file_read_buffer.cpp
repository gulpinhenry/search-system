#include "file_read_buffer.h"
#include <cinttypes>
#include <iostream>
#include <cstring>

bool FileReadBuffer::readPairToVector(std::ifstream &tempFile, const int &fileIndex,
                                      std::vector<Tuple> &records, int n, std::size_t chunkSize) // return if there is remain
{

    // Reserve space for n records
    records.clear();
    int targetLen = n;

    std::vector<char> &buffer = readBuffer;
    std::size_t remainingBytes = 0; // Keep track of remaining data

    while (true)
    {
        // Read a chunk of data
        tempFile.read(buffer.data() + remainingBytes, chunkSize - remainingBytes);
        std::streamsize bytesRead = tempFile.gcount();

        // If no bytes were read, check for end-of-file or error
        if (bytesRead <= 0)
        {
            if (tempFile.eof() || tempFile.peek() == EOF)
            {
                if (records.size() == 0)
                    valid = false;
                return false;
            }
            else
            {
                std::cerr << "Error reading from file index " << fileIndex << std::endl;
                exit(-3); // Return on read error
            }
        }
        if (bytesRead < 5)
        {
            std::cout << "Reading little amount of byte check whether its error" << bytesRead << std::endl;
        }

        std::size_t totalBytesRead = remainingBytes + bytesRead;
        std::size_t mainOffset = 0;

        // Process the chunk until we either run out of data or reach n records
        while (mainOffset < totalBytesRead && records.size() < (targetLen))
        {
            std::size_t tempOffset = mainOffset; // Temp offset for this iteration

            if (tempOffset + sizeof(uint16_t) > totalBytesRead)
                break; // Check for termLength

            uint16_t termLength;
            std::memcpy(&termLength, buffer.data() + tempOffset, sizeof(termLength));
            tempOffset += sizeof(termLength);

            if (tempOffset + termLength > totalBytesRead)
                break; // Check for term data

            std::string term(buffer.data() + tempOffset, termLength);
            tempOffset += termLength;

            if (tempOffset + sizeof(int) > totalBytesRead)
                break; // Check for docID

            int docID;
            std::memcpy(&docID, buffer.data() + tempOffset, sizeof(docID));
            tempOffset += sizeof(docID);

            if (tempOffset + sizeof(float) > totalBytesRead)
                break; // Check for termFreqScore

            float termFreqScore;
            std::memcpy(&termFreqScore, buffer.data() + tempOffset, sizeof(termFreqScore));
            tempOffset += sizeof(termFreqScore);

            // Only add the record if all fields were successfully read
            records.emplace_back(term, docID, fileIndex, termFreqScore);
            // Update the mainOffset only after a successful addition
            mainOffset = tempOffset;
        }

        // Handle remaining data
        remainingBytes = totalBytesRead - mainOffset; // Calculate remaining bytes after parsing
        if (records.size() < targetLen)
        {
            if (remainingBytes > 0)
            {
                std::memcpy(buffer.data(), buffer.data() + mainOffset, remainingBytes); // Move remaining data to the front
            }

            // If there is still space in the buffer, read more data this should always be true
            if (remainingBytes < chunkSize)
            {
                continue; // The next read will fill the buffer
            }
            else
            {
                std::cerr << "Not parsing? remaining bytes == chunkSize try set larger chunkSize" << std::endl;
            }
        }
        else // records reach target len
        {
            if (remainingBytes > 0)
            {
                tempFile.seekg(-(std::streamoff)(remainingBytes), std::ios::cur);
            }
            return true;
        }
    }
}

void FileReadBuffer::fillBuffer()
{
    tupleBuffer.clear();
    if (valid)
    {
        if (!end)
        {
            curPos = 0;

            if (!readPairToVector(fileStream, fileIndex, tupleBuffer, maxSize, chunkSize))
                end = true;
        }
        else
        {
            valid = false;
        }
    }
}

FileReadBuffer::FileReadBuffer(const std::string &filename, const int &fileIndex,
                               const int &maxSize, const size_t &chunkSize) : tupleBuffer(), maxSize(maxSize),
                                                                              fileIndex(fileIndex), chunkSize(chunkSize), readBuffer(chunkSize)
{
    valid = true;
    end = false;
    fileStream.open(filename, std::ios::binary);
    if (!fileStream.is_open())
    {
        valid = false;
        return;
    }
    fillBuffer();
}

FileReadBuffer::~FileReadBuffer()
{
    fileStream.close();
}

bool FileReadBuffer::isValid()
{
    return valid;
}

void FileReadBuffer::close()
{
    valid = false;
}

Tuple FileReadBuffer::getOneRecord()
{
    if (valid)
    {
        int size = tupleBuffer.size();
        if (curPos < size)
        {
            if (end && (curPos == size - 1))
                valid = false;
            return tupleBuffer[curPos++];
        }
        else
        {
            fillBuffer();
            if (valid)
                return tupleBuffer[curPos++];
            else
            {
                // std::cerr << "Reading without checking valid" << tupleBuffer.size() << std::endl;
                return Tuple();
            }
        }
    }
    return Tuple();
}

Tuple FileReadBuffer::jumpTo(const std::string &term) // get first term equal or larger then term
{
    if (term == "") return getOneRecord();
    auto record = getOneRecord();
    while (tupleBuffer.size() != 0 && std::get<0>(tupleBuffer[tupleBuffer.size() - 1]) < term)
    {
        fillBuffer();
    }
    while (record != Tuple() && std::get<0>(record) < term) {
        record = getOneRecord();
    }
    return record;
}
