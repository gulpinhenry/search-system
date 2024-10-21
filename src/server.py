from flask import Flask, request, jsonify
import subprocess
import threading
import queue
import sys

app = Flask(__name__)

# Queue to communicate with the C++ process
query_queue = queue.Queue()
output_queue = queue.Queue()

# Function to run the C++ query processor
def run_query_processor():
    process = subprocess.Popen(
        ['../build/query_processor'],  # Path to the compiled C++ executable
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    while True:
        try:
            # Get the query and mode from the queue
            query, mode = query_queue.get()
            if query is None:  # Check for termination signal
                break
            
            # Prepare input for the C++ program
            input_data = f"{query}\n{mode}\n"
            stdout, stderr = process.communicate(input=input_data, timeout=10)

            # Debugging output
            if stdout:
                print(f"C++ Output:\n{stdout}", file=sys.stderr)
            if stderr:
                print(f"C++ Error:\n{stderr}", file=sys.stderr)

            if process.returncode != 0:
                print(f"Error: {stderr}")
                output_queue.put([])  # Return empty results on error
            else:
                results = []
                for line in stdout.splitlines():
                    if line.strip() and line.startswith('DocID:'):
                        parts = line.split(',')
                        doc_id = parts[0].split(':')[1].strip()
                        doc_name = parts[1].split(':')[1].strip()
                        score = parts[2].split(':')[1].strip()
                        results.append({'docID': doc_id, 'docName': doc_name, 'score': score})

                output_queue.put(results)  # Store results in the output queue

        except Exception as e:
            print(f"Exception: {e}", file=sys.stderr)
            output_queue.put([])  # Return empty results on exception

@app.route('/search', methods=['POST'])
def search():
    data = request.json
    query = data.get('query')
    mode = data.get('mode', 'OR').upper()  # Default to OR if not specified

    # Send the query and mode to the C++ process
    query_queue.put((query, mode))

    # Wait for results from the C++ process
    results = output_queue.get()
    return jsonify(results)

if __name__ == '__main__':
    # Start the C++ query processor in a separate thread
    threading.Thread(target=run_query_processor, daemon=True).start()
    
    app.run(debug=True, host='0.0.0.0', port=5000)