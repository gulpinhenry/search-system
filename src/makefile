CXX = g++
CXXFLAGS = -Wall -std=c++17 -I../include

all: parser_and_indexer merger query_processor

parser_and_indexer: parser_and_indexer.cpp compression.cpp
	$(CXX) $(CXXFLAGS) -o ../build/parser_and_indexer parser_and_indexer.cpp compression.cpp
	../build/parser_and_indexer

merger: merger.cpp compression.cpp
	$(CXX) $(CXXFLAGS) -o ../build/merger merger.cpp compression.cpp
	../build/merger

query_processor: query_processor.cpp compression.cpp bm25.cpp
	$(CXX) $(CXXFLAGS) -o ../build/query_processor query_processor.cpp compression.cpp bm25.cpp
	../build/query_processor

test: test_bin_reader.cpp
	$(CXX) $(CXXFLAGS) -o ../build/test_bin_reader test_bin_reader.cpp compression.cpp
	../build/test_bin_reader

clean:
	rm -f ../build/parser_and_indexer ../build/merger ../build/query_processor ../build/test_bin_reader
	rm -f ../logs/*.log
	rm -f ../data/intermediate/*.bin ../data/intermediate/*.txt ../data/intermediate/*.idx ../data/*.bin
