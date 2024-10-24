from flask import Flask, request, jsonify, send_from_directory
import subprocess
import threading
import queue
import sys
import time
import os

app = Flask(__name__, static_folder='static')

process = None
response_queue = queue.Queue()  # Queue to store query results

def start_query_processor():
    """Start the C++ query processor."""
    global process
    try:
        print("Starting query processor...", file=sys.stderr)
        process = subprocess.Popen(
            ['../build/query_processor'],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
            env={**os.environ, "PYTHONUNBUFFERED": "1"}
        )
        print("Waiting for query processor to initialize...", file=sys.stderr)
        time.sleep(20)  # Wait for the query processor to start
        print("Query processor ready.", file=sys.stderr)

        # Start the background thread to read from the process
        threading.Thread(target=read_from_process, daemon=True).start()
    except Exception as e:
        print(f"Error starting query processor: {e}", file=sys.stderr)
        sys.exit(1)

def read_from_process():
    """Continuously read from the process and store relevant output."""
    current_output = []  # Store output for the current query
    while True:
        line = process.stdout.readline().strip()
        if line:
            print(f"DEBUG: {line}", file=sys.stderr)
            if "DocID:" in line:
                parts = line.split(',')
                doc_id = parts[0].split(':')[1].strip()
                doc_name = parts[1].split(':')[1].strip()
                score = parts[2].split(':')[1].strip()
                current_output.append({'docID': doc_id, 'docName': doc_name, 'score': score})
            elif "Enter your query" in line:
                response_queue.put(current_output)
                current_output = []

def send_query_and_mode(query, mode):
    """Send the query and mode to the C++ process."""
    try:
        process.stdin.write(query + "\n")
        process.stdin.flush()

        process.stdin.write(mode + "\n")
        process.stdin.flush()

        print("Sent query and mode. Waiting for response...", file=sys.stderr)

        results = response_queue.get(timeout=10)  # 10-second timeout
        return results

    except queue.Empty:
        print("No response received within the timeout period.", file=sys.stderr)
        return []

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return []

@app.route('/')
def index():
    """Serve the front-end HTML."""
    return send_from_directory(app.static_folder, 'index.html')

@app.route('/search', methods=['POST'])
def search():
    """Handle search requests."""
    data = request.json
    query = data.get('query', '')
    mode = data.get('mode', 'OR').upper()

    if not query:
        return jsonify({"error": "Query cannot be empty"}), 400

    results = send_query_and_mode(query, mode)
    return jsonify(results)

if __name__ == '__main__':
    start_query_processor()
    try:
        app.run(debug=True, use_reloader=False, host='0.0.0.0', port=5000)
    except Exception as e:
        print(f"Error starting Flask server: {e}", file=sys.stderr)
