CXX = g++
CXXFLAGS = -Wall -std=c++17 -I../include -g


all: clean parser_and_indexer_mt merger query_processor


parser_and_indexer_mt: parser_and_indexer_mt.cpp compression.cpp utils.cpp
	$(CXX) $(CXXFLAGS) -g -o ../build/parser_and_indexer_mt parser_and_indexer_mt.cpp compression.cpp  utils.cpp thread_pool.cpp -lpthread
	../build/parser_and_indexer_mt	

merger: merger.cpp compression.cpp
	$(CXX) $(CXXFLAGS) -o ../build/merger merger.cpp compression.cpp
	../build/merger

merger_mt: merge_temp_file.cpp thread_pool.cpp file_read_buffer.cpp inverted_index.cpp
	$(CXX) $(CXXFLAGS) -g -o ../build/temp_file_merger merge_temp_file.cpp thread_pool.cpp file_read_buffer.cpp compression.cpp inverted_index.cpp -lpthread
	../build/temp_file_merger

query_processor: query_processor.cpp compression.cpp
	$(CXX) $(CXXFLAGS) -o ../build/query_processor query_processor.cpp compression.cpp inverted_index.cpp
	../build/query_processor

test_parse: test_bin_reader.cpp
	$(CXX) $(CXXFLAGS) -o ../build/test_bin_reader test_bin_reader.cpp compression.cpp
	../build/test_bin_reader

test_merge: test_merger.cpp 
	$(CXX) $(CXXFLAGS) -o ../build/test_merger test_merger.cpp
	../build/test_merger
clean:
	rm -f ../build/parser_and_indexer ../build/merger ../build/query_processor ../build/test_bin_reader
	rm -f ../logs/*.log
	rm -f ../data/intermediate/*.bin ../data/intermediate/*.txt ../data/intermediate/*.idx ../data/*.bin
