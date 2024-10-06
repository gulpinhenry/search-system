CXX = g++
CXXFLAGS = -Wall -std=c++11 -I../include

all: parser_and_indexer merger query_processor

parser_and_indexer: parser_and_indexer.cpp compression.cpp
	$(CXX) $(CXXFLAGS) -o ../build/parser_and_indexer parser_and_indexer.cpp compression.cpp

merger: merger.cpp compression.cpp
	$(CXX) $(CXXFLAGS) -o ../build/merger merger.cpp compression.cpp

query_processor: query_processor.cpp compression.cpp bm25.cpp
	$(CXX) $(CXXFLAGS) -o ../build/query_processor query_processor.cpp compression.cpp bm25.cpp

clean:
	rm -f ../build/parser_and_indexer ../build/merger ../build/query_processor
