import os
import queue
import subprocess
import threading

from flask import Flask, render_template, jsonify, request

app = Flask(__name__)

# Resolve paths relative to this file.
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ENGINE_PATH = os.path.join(BASE_DIR, "engine", "nox_engine")

# Engine state
engine_process = None
engine_queue = queue.Queue()
engine_lock = threading.Lock()


def read_engine_output(process, q):
    """Continuously read stdout from the chess engine."""
    while True:
        line = process.stdout.readline()

        if not line:
            break

        q.put(line.strip())


def init_engine():
    """Start the UCI chess engine."""
    global engine_process

    # Another request may have initialized it while we were waiting.
    if engine_process and engine_process.poll() is None:
        return True

    if not os.path.exists(ENGINE_PATH):
        print(f"Engine binary not found: {ENGINE_PATH}")
        return False

    # Make sure the binary is executable.
    try:
        os.chmod(ENGINE_PATH, 0o755)
    except OSError as e:
        print(f"Warning: could not chmod engine: {e}")

    try:
        engine_process = subprocess.Popen(
            [ENGINE_PATH],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

        thread = threading.Thread(
            target=read_engine_output,
            args=(engine_process, engine_queue),
            daemon=True,
        )
        thread.start()

        # Initialize UCI.
        send_uci_command("uci")
        send_uci_command("isready")

        print("Chess engine started successfully.")
        return True

    except Exception as e:
        print(f"Failed to start engine: {e}")
        engine_process = None
        return False


def send_uci_command(cmd):
    """Send a command to the UCI engine."""
    if (
        engine_process
        and engine_process.poll() is None
        and engine_process.stdin
    ):
        engine_process.stdin.write(f"{cmd}\n")
        engine_process.stdin.flush()


def clear_engine_queue():
    """Remove stale engine output."""
    while True:
        try:
            engine_queue.get_nowait()
        except queue.Empty:
            break


def get_engine_response(timeout=10.0, stop_condition=None):
    """Read engine output until timeout or stop condition."""
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


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/move", methods=["POST"])
def move():
    data = request.get_json(silent=True)

    if not data:
        return jsonify({"error": "Invalid JSON request"}), 400

    fen = data.get("fen")

    if not fen:
        return jsonify({"error": "Missing FEN"}), 400

    movetime = data.get("movetime", 1000)
    wtime = data.get("wtime")
    btime = data.get("btime")

    # Protect the single engine process from concurrent requests.
    with engine_lock:

        # Start engine lazily when the first /move request arrives.
        if (
            engine_process is None
            or engine_process.poll() is not None
        ):
            if not init_engine():
                return jsonify({
                    "error": "Engine binary not found or failed to initialize"
                }), 500

        # Remove output from previous commands.
        clear_engine_queue()

        try:
            # Set the current position.
            send_uci_command(f"position fen {fen}")

            # Start the search.
            if wtime is not None and btime is not None:
                send_uci_command(
                    f"go wtime {wtime} btime {btime}"
                )
            else:
                send_uci_command(
                    f"go movetime {movetime}"
                )

            # Wait for bestmove.
            output = get_engine_response(
                timeout=10.0,
                stop_condition=lambda line: line.startswith("bestmove"),
            )

            for line in output:
                if line.startswith("bestmove"):
                    parts = line.split()

                    if len(parts) >= 2:
                        return jsonify({
                            "bestmove": parts[1]
                        })

            return jsonify({
                "error": "Engine did not return a move"
            }), 500

        except Exception as e:
            print(f"Engine error: {e}")

            return jsonify({
                "error": "Failed to communicate with chess engine"
            }), 500


# Used only when running locally.
# Vercel imports `app` directly and does not execute this block.
if __name__ == "__main__":
    print(f"Engine path: {ENGINE_PATH}")

    if init_engine():
        print("Engine initialized.")
    else:
        print("Engine could not be initialized.")

    app.run(
        host="0.0.0.0",
        port=5000,
        debug=True,
    )

