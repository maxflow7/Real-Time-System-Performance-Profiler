# server.py
from flask import Flask, jsonify, send_from_directory
import time
import os
import csv
from threading import Lock
from collections import deque

# A simple Flask server to serve performance data from a CSV file and put the thread lock ensure safe access to shared data.

app = Flask(__name__)
DATA_FILE = '/tmp/perf_data.csv'
DATA_HISTORY = deque(maxlen=1000)  # Keep last 1000 samples
DATA_LOCK = Lock()

def read_latest_data():
    """Read the latest data from the CSV file"""
    if not os.path.exists(DATA_FILE):
        return []
    
    data = []
    with DATA_LOCK:
        with open(DATA_FILE, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                data.append({
                    'timestamp': int(row['timestamp']),
                    'cycles': int(row['cycles']),
                    'instructions': int(row['instructions']),
                    'cache_misses': int(row['cache_misses']),
                    'branch_misses': int(row['branch_misses']),
                    'cpi': float(row['cpi'])
                })
    
    # Keep only the latest data in memory
    DATA_HISTORY.extend(data)
    return list(DATA_HISTORY)


# serves the static files from the 'static' directory when the root url is accessed.
@app.route('/')
def index():
    return send_from_directory('static', 'index.html')

@app.route('/api/data')
def get_data():
    data = read_latest_data()
    return jsonify(data)

@app.route('/api/latest')
def get_latest():
    data = read_latest_data()
    return jsonify(data[-1] if data else {})

if __name__ == '__main__':
    # Create static directory if it doesn't exist
    os.makedirs('static', exist_ok=True)
    
    # Start the Flask app
    app.run(host='0.0.0.0', port=5000, debug=True)
