#!/usr/bin/env python3
import argparse, subprocess, threading, queue, time, sys
import chess

class UciEngine:
    def __init__(self, path):
        self.p = subprocess.Popen([path], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=1, universal_newlines=True)
        self.q = queue.Queue()
        self.t = threading.Thread(target=self._reader, daemon=True)
        self.t.start()
        self.send('uci')
        self._wait_for('uciok')
        self.send('isready')
        self._wait_for('readyok')

    def _reader(self):
        for line in self.p.stdout:
            self.q.put(line.rstrip('\n'))

    def send(self, s):
        self.p.stdin.write(s + '\n')
        self.p.stdin.flush()

    def _wait_for(self, token, timeout=5.0):
        deadline = time.time() + timeout
        out = []
        while time.time() < deadline:
            try:
                line = self.q.get(timeout=0.05)
                out.append(line)
                if token in line:
                    return out
            except queue.Empty:
                pass
        raise TimeoutError(f"Timeout waiting for {token}. Got: {out[-5:]}" )

    def setoption(self, name, value):
        self.send(f"setoption name {name} value {value}")

    def newgame(self):
        self.send('ucinewgame')
        self.send('isready')
        self._wait_for('readyok')

    def position_start(self, moves=None):
        if moves:
            self.send('position startpos moves ' + ' '.join(moves))
        else:
            self.send('position startpos')

    def go_movetime(self, ms):
        self.send(f"go movetime {ms}")
        # read until bestmove
        best = None
        while True:
            line = self.q.get()
            if line.startswith('bestmove'):
                parts = line.split()
                best = parts[1] if len(parts) > 1 else '0000'
                break
        return best

    def evalfen(self, fen):
        self.send(f"evalfen {fen}")
        # last number echoed as a single line; read lines until we get a numeric-looking one
        while True:
            line = self.q.get()
            # try to parse number
            try:
                return float(line.strip())
            except Exception:
                # skip non-numeric
                if line.startswith('bestmove'):
                    # shouldn't happen during evalfen, but avoid deadlocks
                    continue
                # keep reading
                pass

    def quit(self):
        try:
            self.send('quit')
            self.p.wait(timeout=1)
        except Exception:
            self.p.kill()


def apply_common_options(eng, threads=1, hmb=16):
    eng.setoption('Threads', threads)
    eng.setoption('Hash', hmb)


def play_game(engineA, engineB, movetime_ms=200, max_plies=120):
    # engineA plays White, engineB plays Black
    moves = []
    side = 0  # 0=A (White), 1=B (Black)
    for ply in range(max_plies):
        if side == 0:
            engineA.position_start(moves)
            bm = engineA.go_movetime(movetime_ms)
        else:
            engineB.position_start(moves)
            bm = engineB.go_movetime(movetime_ms)
        if bm == '0000' or bm == '(none)' or len(bm) < 4:
            break
        moves.append(bm)
        side ^= 1
    # Return the final moves list
    return moves


def moves_to_fen(moves):
    board = chess.Board()
    for u in moves:
        try:
            board.push_uci(u)
        except Exception:
            break
    return board.fen(), board


def result_from_board(engine_ref, board: chess.Board, threshold_cp=200):
    # Exact game termination adjudication first
    if board.is_game_over():
        res = board.result()
        if res == '1-0':
            return 1.0  # White win
        if res == '0-1':
            return 0.0  # Black win (White loses)
        return 0.5      # Draw
    # Otherwise, evaluate the final FEN using classical eval and threshold
    fen = board.fen()
    engine_ref.setoption('Use NNUE', 'false')
    try:
        sc = engine_ref.evalfen(fen)  # cp from side-to-move perspective
    except Exception:
        return 0.5
    # Positive means side-to-move is better; map to White outcome
    # Determine side to move from FEN
    stm_white = (' w ' in (' ' + fen + ' '))
    # If stm is White and score>thr => White win; if stm is Black and score<-thr => White win
    if abs(sc) < threshold_cp:
        return 0.5
    white_better = (sc > 0 and stm_white) or (sc < 0 and not stm_white)
    return 1.0 if white_better else 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--engine', required=True)
    ap.add_argument('--evalfile', required=True, help='NOXNET .nox for NN-on side')
    ap.add_argument('--games', type=int, default=10)
    ap.add_argument('--movetime', type=int, default=200)
    ap.add_argument('--max-plies', type=int, default=120)
    ap.add_argument('--threads', type=int, default=1)
    ap.add_argument('--hash', type=int, default=16)
    args = ap.parse_args()

    nn_wins = 0.0
    total = 0

    for g in range(args.games):
        # A: NN on, B: NN off
        A = UciEngine(args.engine)
        B = UciEngine(args.engine)
        apply_common_options(A, args.threads, args.hash)
        apply_common_options(B, args.threads, args.hash)
        A.setoption('Use NNUE', 'true')
        A.setoption('EvalFile', args.evalfile)
        B.setoption('Use NNUE', 'false')
        A.newgame(); B.newgame()

        # Swap colors every game
        nn_as_white = (g % 2 == 0)
        if nn_as_white:
            moves = play_game(A, B, args.movetime, args.max_plies)
        else:
            moves = play_game(B, A, args.movetime, args.max_plies)

        # Adjudicate by rules or eval
        fen, board = moves_to_fen(moves)
        ref = UciEngine(args.engine)
        apply_common_options(ref, args.threads, args.hash)
        score_white = result_from_board(ref, board)
        ref.quit()

        # Convert to NN score depending on its color
        if nn_as_white:
            nn_score = score_white
        else:
            nn_score = 1.0 - score_white

        nn_wins += nn_score
        total += 1
        print(f"game {g+1}/{args.games}: plies={len(moves)} nn_as_white={nn_as_white} nn_score={nn_score:.2f}")

        A.quit(); B.quit()

    print(f"NN score: {nn_wins:.1f} / {total} = {nn_wins/total:.3f}")

if __name__ == '__main__':
    main()
