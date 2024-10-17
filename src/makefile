CXX = g++
CXXFLAGS = -Wall -std=c++17 -I../include

build_and_run_all: all run_parser_and_indexer run_merger run_query_processor

all: parser_and_indexer merger query_processor


parser_and_indexer_mt: parser_and_indexer_mt.cpp compression.cpp utils.cpp
	$(CXX) $(CXXFLAGS) -o ../build/parser_and_indexer_mt parser_and_indexer_mt.cpp compression.cpp  utils.cpp thread_pool.cpp	

parser_and_indexer: parser_and_indexer.cpp compression.cpp utils.cpp
	$(CXX) $(CXXFLAGS) -o ../build/parser_and_indexer parser_and_indexer.cpp compression.cpp utils.cpp

run_parser_and_indexer:
	../build/parser_and_indexer

merger: merger.cpp compression.cpp
	$(CXX) $(CXXFLAGS) -o ../build/merger merger.cpp compression.cpp

run_merger:
	../build/merger

query_processor: query_processor.cpp inverted_index.cpp compression.cpp
	$(CXX) $(CXXFLAGS) -o ../build/query_processor query_processor.cpp inverted_index.cpp compression.cpp 

run_query_processor:
	../build/query_processor

test_parse: test_bin_reader.cpp
	$(CXX) $(CXXFLAGS) -o ../build/test_bin_reader test_bin_reader.cpp compression.cpp
	../build/test_bin_reader

test_merge: test_merger.cpp
	$(CXX) $(CXXFLAGS) -o ../build/test_merger test_merger.cpp compression.cpp
	../build/test_merger
clean:
	rm -f ../build/parser_and_indexer ../build/merger ../build/query_processor ../build/test_bin_reader
	rm -f ../logs/*.log
	rm -f ../data/intermediate/*.bin ../data/intermediate/*.txt ../data/intermediate/*.idx ../data/*.bin
