import os
import subprocess
import threading
import queue
from flask import Flask, render_template, jsonify, request

app = Flask(__name__)

# CONFIGURATION: Update this to point to your engine binary
ENGINE_PATH = "/home/noxious/Desktop/open_source/nox_engine/build/nox_engine"  # e.g., "./arena_engine.exe" or "./my_engine"

engine_process = None
engine_queue = queue.Queue()

def read_engine_output(process, q):
    while True:
        line = process.stdout.readline()
        if not line:
            break
        q.put(line.strip())

def init_engine():
    global engine_process
    if not os.path.exists(ENGINE_PATH):
        print(f"Warning: Engine binary not found at {ENGINE_PATH}. Please update configuration.")
        return False
        
    try:
        engine_process = subprocess.Popen(
            ENGINE_PATH,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )
        
        t = threading.Thread(target=read_engine_output, args=(engine_process, engine_queue), daemon=True)
        t.start()
        
        send_uci_command("uci")
        send_uci_command("isready")
        return True
    except Exception as e:
        print(f"Failed to start engine: {e}")
        return False

def send_uci_command(cmd):
    if engine_process and engine_process.stdin:
        engine_process.stdin.write(f"{cmd}\n")
        engine_process.stdin.flush()

def get_engine_response(timeout=2.0, stop_condition=None):
    lines = []
    while True:
        try:
            line = engine_queue.get(timeout=timeout)
            lines.append(line)
            if stop_condition and stop_condition(line):
                break
        except queue.Empty:
            break
    return lines

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/move', methods=['POST'])
def move():
    data = request.get_json()
    fen = data.get('fen')
    movetime = data.get('movetime', 1000)
    wtime = data.get('wtime')
    btime = data.get('btime')
    
    if not engine_process:
        if not init_engine():
            return jsonify({'error': 'Engine binary not found or failed to initialize'}), 500

    # Clear old queue elements before generating new move
    while not engine_queue.empty():
        try: engine_queue.get_nowait()
        except queue.Empty: break

    # Send position and ask for best move
    send_uci_command(f"position fen {fen}")
    
    # Send time controls to engine if provided, otherwise default to movetime
    if wtime is not None and btime is not None:
        send_uci_command(f"go wtime {wtime} btime {btime}")
    else:
        send_uci_command(f"go movetime {movetime}")
    
    output = get_engine_response(timeout=10.0, stop_condition=lambda l: l.startswith('bestmove'))
    
    bestmove = None
    for line in output:
        if line.startswith('bestmove'):
            bestmove = line.split()[1]
            break
            
    if bestmove:
        return jsonify({'bestmove': bestmove})
    return jsonify({'error': 'Engine did not return a move'}), 500

if __name__ == '__main__':
    init_engine()
    app.run(debug=True, port=5000)

