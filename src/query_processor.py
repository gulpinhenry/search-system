import struct
import math
from collections import defaultdict, deque

# Constants for BM25
k1 = 1.5
b = 0.75

# --- InvertedListPointer Implementation ---

class InvertedListPointer:
    def __init__(self, index_file, lex_entry):
        self.index_file = index_file
        self.lex_entry = lex_entry
        self.current_doc_id = -1
        self.valid = True
        self.last_doc_id = 0
        self.buffer_pos = 0
        self.compressed_data = bytearray(lex_entry['length'])
        index_file.seek(lex_entry['offset'])
        index_file.readinto(self.compressed_data)

    def next(self):
        if not self.valid:
            return False
        
        if self.buffer_pos >= len(self.compressed_data):
            self.valid = False
            return False

        gap = self.varbyte_decode_number()
        self.last_doc_id += gap
        self.current_doc_id = self.last_doc_id
        return True

    def next_geq(self, doc_id):
        while self.valid and self.current_doc_id < doc_id:
            if not self.next():
                return False
        return self.valid

    def get_doc_id(self):
        return self.current_doc_id

    def get_tf(self):
        return 1  # TF is not stored, return a default value

    def is_valid(self):
        return self.valid

    def close(self):
        self.valid = False

    def varbyte_decode_number(self):
        # Implement varbyte decoding
        # Placeholder function
        return 1  # Dummy return for the sake of example

# --- InvertedIndex Implementation ---

class InvertedIndex:
    def __init__(self, index_filename, lexicon_filename):
        self.lexicon = {}
        self.index_file = open(index_filename, 'rb')
        self.load_lexicon(lexicon_filename)

    def load_lexicon(self, lexicon_filename):
        with open(lexicon_filename, 'rb') as lexicon_file:
            while True:
                term_length_bytes = lexicon_file.read(2)
                if not term_length_bytes:
                    break
                term_length = struct.unpack('H', term_length_bytes)[0]
                term = lexicon_file.read(term_length).decode('utf-8')

                entry = {
                    'offset': struct.unpack('I', lexicon_file.read(4))[0],
                    'length': struct.unpack('I', lexicon_file.read(4))[0],
                    'doc_frequency': struct.unpack('I', lexicon_file.read(4))[0],
                    'block_count': struct.unpack('I', lexicon_file.read(4))[0]
                }
                self.lexicon[term] = entry

    def open_list(self, term):
        return term in self.lexicon

    def get_list_pointer(self, term):
        return InvertedListPointer(self.index_file, self.lexicon[term])

    def close_list(self, term):
        pass  # No action needed

# --- QueryProcessor Implementation ---

class QueryProcessor:
    def __init__(self, index_filename, lexicon_filename, page_table_filename, doc_lengths_filename):
        self.inverted_index = InvertedIndex(index_filename, lexicon_filename)
        self.page_table = {}
        self.doc_lengths = {}
        self.total_docs = 0
        self.avg_doc_length = 0
        self.load_page_table(page_table_filename)
        self.load_document_lengths(doc_lengths_filename)

        self.total_docs = len(self.doc_lengths)
        # Print the total number of documents
        print(f"Total Documents: {self.total_docs}")
        
        total_doc_length = sum(self.doc_lengths.values())
        self.avg_doc_length = total_doc_length / self.total_docs if self.total_docs > 0 else 0

    def parse_query(self, query):
        terms = query.split()
        return [term.strip('.,!?').lower() for term in terms if term]

    def load_page_table(self, page_table_filename):
        with open(page_table_filename, 'rb') as page_table_file:
            while True:
                doc_id_bytes = page_table_file.read(4)
                if not doc_id_bytes:
                    break
                doc_id = struct.unpack('I', doc_id_bytes)[0]
                name_length = struct.unpack('H', page_table_file.read(2))[0]
                doc_name = page_table_file.read(name_length).decode('utf-8')
                self.page_table[doc_id] = doc_name

    def load_document_lengths(self, doc_lengths_filename):
        with open(doc_lengths_filename, 'rb') as doc_lengths_file:
            while True:
                doc_id_bytes = doc_lengths_file.read(4)
                if not doc_id_bytes:
                    break
                doc_id = struct.unpack('I', doc_id_bytes)[0]
                doc_length = struct.unpack('I', doc_lengths_file.read(4))[0]
                self.doc_lengths[doc_id] = doc_length

    def process_query(self, query, conjunctive):
        terms = self.parse_query(query)
        if not terms:
            print("No terms found in query.")
            return

        term_pointers = []
        for term in terms:
            if not self.inverted_index.open_list(term):
                print(f"Term not found: {term}")
                continue
            term_pointers.append((term, self.inverted_index.get_list_pointer(term)))

        if not term_pointers:
            print("No valid terms found in query.")
            return

        # DAAT Processing
        doc_scores = defaultdict(float)

        if conjunctive:
            doc_ids = []
            for tp in term_pointers:
                ptr = tp[1]
                if not ptr.is_valid() or not ptr.next():
                    ptr.close()
                    return  # One of the lists is empty
                doc_ids.append(ptr.get_doc_id())

            while True:
                max_doc_id = max(doc_ids)
                all_match = True

                for i, tp in enumerate(term_pointers):
                    ptr = tp[1]
                    while doc_ids[i] < max_doc_id:
                        if not ptr.next_geq(max_doc_id):
                            all_match = False
                            break
                        doc_ids[i] = ptr.get_doc_id()
                    if doc_ids[i] != max_doc_id:
                        all_match = False

                if not all_match:
                    if any(not tp[1].is_valid() for tp in term_pointers):
                        break
                    continue

                doc_id = max_doc_id
                total_score = 0.0
                for tp in term_pointers:
                    ptr = tp[1]
                    term = tp[0]
                    tf = ptr.get_tf()
                    doc_length = self.doc_lengths[doc_id]
                    df = self.inverted_index.lexicon[term]['doc_frequency']
                    idf = math.log((self.total_docs - df + 0.5) / (df + 0.5)) if df > 0 else 0
                    K = k1 * ((1 - b) + b * (doc_length / self.avg_doc_length))
                    bm25_score = idf * ((k1 + 1) * tf) / (K + tf)
                    total_score += bm25_score
                    ptr.next()  # Advance pointer for next iteration
                doc_scores[doc_id] += total_score

                valid_pointers = all(ptr.is_valid() for tp in term_pointers for ptr in [tp[1]])
                if not valid_pointers:
                    break

        else:
            # Disjunctive query processing
            pq = []
            for tp in term_pointers:
                ptr = tp[1]
                if ptr.is_valid() and ptr.next():
                    pq.append((ptr, tp[0]))

            while pq:
                pq.sort(key=lambda x: x[0].get_doc_id())
                ptr, term = pq.pop(0)
                doc_id = ptr.get_doc_id()
                tf = ptr.get_tf()
                doc_length = self.doc_lengths[doc_id]
                df = self.inverted_index.lexicon[term]['doc_frequency']
                idf = math.log((self.total_docs - df + 0.5) / (df + 0.5)) if df > 0 else 0
                K = k1 * ((1 - b) + b * (doc_length / self.avg_doc_length))
                bm25_score = idf * ((k1 + 1) * tf) / (K + tf)

                doc_scores[doc_id] += bm25_score

                if ptr.next():
                    pq.append((ptr, term))

        # Display top results
        if doc_scores:
            ranked_docs = sorted(doc_scores.items(), key=lambda x: x[1], reverse=True)
            for i, (doc_id, score) in enumerate(ranked_docs[:10]):
                doc_name = self.page_table[doc_id]
                print(f"{i + 1}. DocID: {doc_id}, DocName: {doc_name}, Score: {score:.4f}")
        else:
            print("No documents matched the query.")

# --- Example Usage ---

def main():
    qp = QueryProcessor('../data/index.bin', '../data/lexicon.bin', '../data/page_table.bin', '../data/doc_lengths.bin')
    
    while True:
        query = input("\nEnter your query (or type 'exit' to quit): ")
        if query.lower() == 'exit':
            break
        mode = input("Choose mode (AND/OR): ")
        conjunctive = mode.lower() == "and"
        qp.process_query(query, conjunctive)

if __name__ == "__main__":
    main()