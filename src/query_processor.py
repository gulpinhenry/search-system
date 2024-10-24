import struct
import math
from collections import defaultdict, deque

document_len = 8841823

def varbyte_encode(number):
    """Encode a single number using varbyte encoding."""
    encoded_number = []
    while number >= 128:
        encoded_number.append((number % 128) | 128)  # Set MSB
        number //= 128
    encoded_number.append(number)  # Final byte with MSB unset
    return encoded_number


def varbyte_encode_list(numbers):
    """Encode a list of numbers using varbyte encoding."""
    encoded = []
    for number in numbers:
        encoded_number = varbyte_encode(number)
        encoded.extend(encoded_number)
    return encoded


def varbyte_decode_list(bytes):
    """Decode a list of bytes back into integers using varbyte decoding."""
    decoded = []
    number = 0
    shift = 0

    for byte in bytes:
        if byte & 128:  # Continuation bit set
            number += (byte & 127) << shift
            shift += 7
        else:  # Last byte of the number
            number += byte << shift
            decoded.append(number)
            number = 0
            shift = 0

    return decoded


def varbyte_decode_number(data, pos):
    """Decode a single number from a list of bytes."""
    number = 0
    shift = 0
    while pos < len(data):
        byte = data[pos]
        pos += 1
        number |= (byte & 0x7F) << shift
        if (byte & 0x80) == 0:  # Continuation bit not set
            break
        shift += 7
    return number

class InvertedListPointer:
    def __init__(self, index_file, lex_entry):
        self.index_file = index_file
        self.lex_entry = lex_entry
        self.current_doc_id = -1
        self.valid = True
        self.last_doc_id = 0
        self.buffer_pos = 0
        self.term_freq_score_index = -1
        self.compressed_data = bytearray(lex_entry['length'])
        self.index_file.seek(lex_entry['offset'])
        self.index_file.readinto(self.compressed_data)
        # Initialize term_freq_score as a mutable bytearray
        self.term_freq_score = bytearray(lex_entry['length'] * 4)  # Assuming float is 4 bytes
        self.index_file.readinto(self.term_freq_score)

        # Convert bytearray to list of floats
        self.term_freq_score = list(struct.unpack(f'{len(self.term_freq_score) // 4}f', self.term_freq_score))
        # self.term_freq_score = [0.0] * lex_entry['length']
        # self.index_file.readinto(struct.pack(f'{len(self.term_freq_score)}f', *self.term_freq_score))

    def next(self):
        if not self.valid:
            return False

        if self.buffer_pos >= len(self.compressed_data):
            self.valid = False
            return False
        self.term_freq_score_index += 1
        if self.term_freq_score_index >= len(self.term_freq_score):
            self.valid = False
            return False

        gap = varbyte_decode_number(self.compressed_data, self.buffer_pos)
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

    def get_tfs(self):
        return self.term_freq_score[self.term_freq_score_index]

    def get_idf(self):
        return self.lex_entry['IDF']

    def is_valid(self):
        return self.valid

    def close(self):
        self.valid = False


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

                term_buffer = lexicon_file.read(term_length)
                term = term_buffer.decode('utf-8')

                entry = {
                    'offset': struct.unpack('Q', lexicon_file.read(8))[0],
                    'length': struct.unpack('I', lexicon_file.read(4))[0],
                    'doc_frequency': struct.unpack('I', lexicon_file.read(4))[0],
                    'block_count': struct.unpack('I', lexicon_file.read(4))[0],
                    'IDF': 0.0,
                    'block_max_doc_ids': [],
                    'block_offsets': []
                }
                entry['IDF'] = math.log((document_len - entry['doc_frequency'] + 0.5) / (entry['doc_frequency'] + 0.5))

                if entry['block_count'] > 0:
                    entry['block_max_doc_ids'] = [struct.unpack('I', lexicon_file.read(4))[0] for _ in range(entry['block_count'])]
                    entry['block_offsets'] = [struct.unpack('Q', lexicon_file.read(8))[0] for _ in range(entry['block_count'])]

                self.lexicon[term] = entry

    def open_list(self, term):
        return term in self.lexicon

    def get_list_pointer(self, term):
        return InvertedListPointer(self.index_file, self.lexicon[term])


class QueryProcessor:
    def __init__(self, index_filename, lexicon_filename, page_table_filename, doc_lengths_filename):
        self.inverted_index = InvertedIndex(index_filename, lexicon_filename)
        self.page_table = {}
        self.doc_lengths = {}
        self.load_page_table(page_table_filename)
        self.load_document_lengths(doc_lengths_filename)
        self.total_docs = len(self.doc_lengths)
        print(f"Total Documents: {self.total_docs}")

        total_doc_length = sum(self.doc_lengths.values())
        self.avg_doc_length = total_doc_length / self.total_docs if self.total_docs > 0 else 0
        print(f"Average Document Length: {self.avg_doc_length}")

    def parse_query(self, query):
        terms = []
        for term in query.split():
            term = ''.join(filter(str.isalnum, term)).lower()
            if term:
                terms.append(term)
        return terms

    def load_page_table(self, page_table_filename):
        with open(page_table_filename, 'rb') as page_table_file:
            while True:
                doc_id_bytes = page_table_file.read(4)
                if not doc_id_bytes:
                    break
                doc_id = struct.unpack('I', doc_id_bytes)[0]

                name_length_bytes = page_table_file.read(2)
                name_length = struct.unpack('H', name_length_bytes)[0]

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

        print(f"Number of Document Lengths Loaded: {len(self.doc_lengths)}")
        for doc_id, length in self.doc_lengths.items():
            if length <= 0:
                print(f"Invalid document length for DocID {doc_id}: {length}")

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

        doc_scores = defaultdict(float)

        if conjunctive:
            doc_ids = []
            for term, ptr in term_pointers:
                if not ptr.is_valid():
                    ptr.close()
                    return
                if not ptr.next():
                    ptr.close()
                    return
                doc_ids.append(ptr.get_doc_id())

            while True:
                max_doc_id = max(doc_ids)
                all_match = True

                for i, (term, ptr) in enumerate(term_pointers):
                    while doc_ids[i] < max_doc_id:
                        if not ptr.next_geq(max_doc_id):
                            all_match = False
                            break
                        doc_ids[i] = ptr.get_doc_id()
                    if doc_ids[i] != max_doc_id:
                        all_match = False

                if not all_match:
                    if any(not ptr.is_valid() for _, ptr in term_pointers):
                        break
                    continue

                doc_id = max_doc_id
                total_score = 0.0
                for term, ptr in term_pointers:
                    bm25_score = ptr.get_idf() * ptr.get_tfs()
                    total_score += bm25_score
                    ptr.next()

                doc_scores[doc_id] = total_score

                valid_pointers = True
                for i, (_, ptr) in enumerate(term_pointers):
                    if ptr.is_valid():
                        doc_ids[i] = ptr.get_doc_id()
                    else:
                        valid_pointers = False
                        break
                if not valid_pointers:
                    break
        else:
            pq = []
            for term, ptr in term_pointers:
                if ptr.is_valid() and ptr.next():
                    pq.append((ptr, term))

            while pq:
                ptr, term = min(pq, key=lambda x: x[0].get_doc_id())
                pq.remove((ptr, term))

                doc_id = ptr.get_doc_id()
                bm25_score = ptr.get_idf() * ptr.get_tfs()
                doc_scores[doc_id] += bm25_score

                if ptr.next():
                    pq.append((ptr, term))

        for _, ptr in term_pointers:
            ptr.close()

        if not doc_scores:
            print("No documents matched the query.")
            return

        ranked_docs = sorted(doc_scores.items(), key=lambda x: x[1], reverse=True)

        results_count = min(10, len(ranked_docs))
        for i in range(results_count):
            doc_id, score = ranked_docs[i]
            doc_name = self.page_table[doc_id]
            print(f"{i + 1}. DocID: {doc_id}, DocName: {doc_name}, Score: {score}")


# Main Function
if __name__ == "__main__":
    qp = QueryProcessor("../data/index.bin", "../data/lexicon.bin", "../data/page_table.bin", "../data/doc_lengths.bin")

    while True:
        query = input("\nEnter your query (or type 'exit' to quit): ")
        if query.lower() == "exit":
            break

        mode = input("Choose mode (AND/OR): ")
        conjunctive = mode.lower() == "and"
        qp.process_query(query, conjunctive)