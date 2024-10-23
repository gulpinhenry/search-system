#ifndef FILE_READ_BUFFER
#define FILE_READ_BUFFER
#include <fstream>
#include <queue>
#include <functional>
#include <string>
#include <vector>

using Tuple = std::tuple<std::string, int, int, float>;

// Define the comparator type using typedef
typedef std::function<bool(const std::tuple<std::string, int, int, float>&, 
                             const std::tuple<std::string, int, int, float>&)> TupleComparator;

// Define the priority queue type using typedef
typedef std::priority_queue<
    std::tuple<std::string, int, int, float>,
    std::vector<std::tuple<std::string, int, int, float>>,
    TupleComparator
> TuplePQ;



class FileReadBuffer
{
private:
    bool valid, end;
    std::vector<std::tuple<std::string, int, int, float>> tupleBuffer;
    std::vector<char> readBuffer;
    std::ifstream fileStream;
    int maxSize, fileIndex;
    size_t chunkSize, curPos;
    void fillBuffer();
    bool readPairToVector(std::ifstream &tempFile, const int &fileIndex,
                      std::vector<Tuple> &records, int n, std::size_t chunkSize);
public:
    FileReadBuffer(const std::string &filename, const int &fileIndex, const int &maxSize, const size_t &chunkSize);
    ~FileReadBuffer();
    bool isValid();
    void close();
    Tuple getOneRecord();
    Tuple jumpTo(const std::string &term);
    FileReadBuffer(FileReadBuffer&&) = default;

};


#endif