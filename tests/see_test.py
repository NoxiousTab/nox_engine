#!/usr/bin/env python3
"""
see_test.py -- static exchange evaluation (SEE) regression test for nox_engine.

Runs the engine's UCI `see <uci_move> <fen>` debug command against a
well-known, publicly maintained test suite of SEE positions (originally
compiled by Thomas Jahn for the Leorik chess engine, and widely reused
across the open-source engine community -- see
https://github.com/lithander/Leorik/blob/master/Leorik.Test/see.epd),
and compares the values nox_engine's Board::see() reports against the
known-correct values.

Note: this test suite -- and nox_engine's Board::see() -- uses simple,
fixed material values (P=100, N=300, B=300, R=500, Q=900), which differ
slightly from eval.cpp's tuned positional values (e.g. N=320, B=330).
This is standard practice: SEE is a tactical material-counting heuristic,
not a positional one, so it conventionally uses simplified round values.

Usage:
    python3 tests/see_test.py                  # run the full suite
    python3 tests/see_test.py --engine PATH     # point at a specific binary

Exit code is 0 if every check passes, 1 otherwise.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

# (FEN without move counters, UCI move, expected SEE value in centipawns)
# One ambiguous source line ("Nxd1" in a position with two knights that
# could both reach d1) was dropped -- it's a typo in the original source
# suite (should be "Ncxd1", which IS included below).
SEE_CASES = [
    ("6k1/1pp4p/p1pb4/6q1/3P1pRr/2P4P/PP1Br1P1/5RKN w - -", "f1f4", -100),
    ("5rk1/1pp2q1p/p1pb4/8/3P1NP1/2P5/1P1BQ1P1/5RK1 b - -", "d6f4", 0),
    ("4R3/2r3p1/5bk1/1p1r3p/p2PR1P1/P1BK1P2/1P6/8 b - -", "h5g4", 0),
    ("4R3/2r3p1/5bk1/1p1r1p1p/p2PR1P1/P1BK1P2/1P6/8 b - -", "h5g4", 0),
    ("4r1k1/5pp1/nbp4p/1p2p2q/1P2P1b1/1BP2N1P/1B2QPPK/3R4 b - -", "g4f3", 0),
    ("2r1r1k1/pp1bppbp/3p1np1/q3P3/2P2P2/1P2B3/P1N1B1PP/2RQ1RK1 b - -", "d6e5", 100),
    ("7r/5qpk/p1Qp1b1p/3r3n/BB3p2/5p2/P1P2P2/4RK1R w - -", "e1e8", 0),
    ("6rr/6pk/p1Qp1b1p/2n5/1B3p2/5p2/P1P2P2/4RK1R w - -", "e1e8", -500),
    ("7r/5qpk/2Qp1b1p/1N1r3n/BB3p2/5p2/P1P2P2/4RK1R w - -", "e1e8", -500),
    ("6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - -", "f7f8q", 200),
    ("6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - -", "f7f8n", 200),
    ("7R/5P2/8/8/6r1/3K4/5p2/4k3 w - -", "f7f8q", 800),
    ("7R/5P2/8/8/6r1/3K4/5p2/4k3 w - -", "f7f8b", 200),
    ("7R/4bP2/8/8/1q6/3K4/5p2/4k3 w - -", "f7f8r", -100),
    ("8/4kp2/2npp3/1Nn5/1p2PQP1/7q/1PP1B3/4KR1r b - -", "h1f1", 0),
    ("8/4kp2/2npp3/1Nn5/1p2P1P1/7q/1PP1B3/4KR1r b - -", "h1f1", 0),
    ("2r2r1k/6bp/p7/2q2p1Q/3PpP2/1B6/P5PP/2RR3K b - -", "c5c1", 100),
    ("r2qk1nr/pp2ppbp/2b3p1/2p1p3/8/2N2N2/PPPP1PPP/R1BQR1K1 w kq -", "f3e5", 100),
    ("6r1/4kq2/b2p1p2/p1pPb3/p1P2B1Q/2P4P/2B1R1P1/6K1 w - -", "f4e5", 0),
    ("3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R4B/PQ3P1P/3R2K1 w - h6", "g5h6", 0),
    ("3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R1B2B/PQ3P1P/3R2K1 w - h6", "g5h6", 100),
    ("2r4r/1P4pk/p2p1b1p/7n/BB3p2/2R2p2/P1P2P2/4RK2 w - -", "c3c8", 500),
    ("2r5/1P4pk/p2p1b1p/5b1n/BB3p2/2R2p2/P1P2P2/4RK2 w - -", "c3c8", 500),
    ("2r4k/2r4p/p7/2b2p1b/4pP2/1BR5/P1R3PP/2Q4K w - -", "c3c5", 300),
    ("8/pp6/2pkp3/4bp2/2R3b1/2P5/PP4B1/1K6 w - -", "g2c6", -200),
    ("4q3/1p1pr1k1/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - -", "e6e4", -400),
    ("4q3/1p1pr1kb/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - -", "h7e4", 100),
    ("3r3k/3r4/2n1n3/8/3p4/2PR4/1B1Q4/3R3K w - -", "d3d4", -100),
    ("1k1r4/1ppn3p/p4b2/4n3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -", "d3e5", 100),
    ("1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -", "d3e5", -200),
    ("rnb2b1r/ppp2kpp/5n2/4P3/q2P3B/5R2/PPP2PPP/RN1QKB2 w Q -", "h4f6", 100),
    ("r2q1rk1/2p1bppp/p2p1n2/1p2P3/4P1b1/1nP1BN2/PP3PPP/RN1QR1K1 b - -", "g4f3", 0),
    ("r1bqkb1r/2pp1ppp/p1n5/1p2p3/3Pn3/1B3N2/PPP2PPP/RNBQ1RK1 b kq -", "c6d4", 0),
    ("r1bq1r2/pp1ppkbp/4N1p1/n3P1B1/8/2N5/PPP2PPP/R2QK2R w KQ -", "e6g7", 0),
    ("r1bq1r2/pp1ppkbp/4N1pB/n3P3/8/2N5/PPP2PPP/R2QK2R w KQ -", "e6g7", 300),
    ("rnq1k2r/1b3ppp/p2bpn2/1p1p4/3N4/1BN1P3/PPP2PPP/R1BQR1K1 b kq -", "d6h2", -200),
    ("rn2k2r/1bq2ppp/p2bpn2/1p1p4/3N4/1BN1P3/PPP2PPP/R1BQR1K1 b kq -", "d6h2", 100),
    ("r2qkbn1/ppp1pp1p/3p1rp1/3Pn3/4P1b1/2N2N2/PPP2PPP/R1BQKB1R b KQq -", "g4f3", 100),
    ("rnbq1rk1/pppp1ppp/4pn2/8/1bPP4/P1N5/1PQ1PPPP/R1B1KBNR b KQ -", "b4c3", 0),
    ("r4rk1/3nppbp/bq1p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - -", "b6b2", -800),
    ("r4rk1/1q1nppbp/b2p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - -", "f6d5", -200),
    ("1r3r2/5p2/4p2p/2k1n1P1/2PN1nP1/1P3P2/8/2KR1B1R b - -", "b8b3", -400),
    ("1r3r2/5p2/4p2p/4n1P1/kPPN1nP1/5P2/8/2KR1B1R b - -", "b8b4", 100),
    ("2r2rk1/5pp1/pp5p/q2p4/P3n3/1Q3NP1/1P2PP1P/2RR2K1 b - -", "c8c1", 0),
    ("5rk1/5pp1/2r4p/5b2/2R5/6Q1/R1P1qPP1/5NK1 b - -", "f5c2", -100),
    ("1r3r1k/p4pp1/2p1p2p/qpQP3P/2P5/3R4/PP3PP1/1K1R4 b - -", "a5a2", -800),
    ("1r5k/p4pp1/2p1p2p/qpQP3P/2P2P2/1P1R4/P4rP1/1K1R4 b - -", "a5a2", 100),
    ("r2q1rk1/1b2bppp/p2p1n2/1ppNp3/3nP3/P2P1N1P/BPP2PP1/R1BQR1K1 w - -", "d5e7", 0),
    ("rnbqrbn1/pp3ppp/3p4/2p2k2/4p3/3B1K2/PPP2PPP/RNB1Q1NR w - -", "d3e4", 100),
    ("rnb1k2r/p3p1pp/1p3p1b/7n/1N2N3/3P1PB1/PPP1P1PP/R2QKB1R w KQkq -", "e4d6", -200),
    ("r1b1k2r/p4npp/1pp2p1b/7n/1N2N3/3P1PB1/PPP1P1PP/R2QKB1R w KQkq -", "e4d6", 0),
    ("2r1k2r/pb4pp/5p1b/2KB3n/4N3/2NP1PB1/PPP1P1PP/R2Q3R w k -", "d5c6", -300),
    ("2r1k2r/pb4pp/5p1b/2KB3n/1N2N3/3P1PB1/PPP1P1PP/R2Q3R w k -", "d5c6", 0),
    ("2r1k3/pbr3pp/5p1b/2KB3n/1N2N3/3P1PB1/PPP1P1PP/R2Q3R w - -", "d5c6", -300),
    ("5k2/p2P2pp/8/1pb5/1Nn1P1n1/6Q1/PPP4P/R3K1NR w KQ -", "d7d8q", 800),
    ("r4k2/p2P2pp/8/1pb5/1Nn1P1n1/6Q1/PPP4P/R3K1NR w KQ -", "d7d8q", -100),
    ("5k2/p2P2pp/1b6/1p6/1Nn1P1n1/8/PPP4P/R2QK1NR w KQ -", "d7d8q", 200),
    ("4kbnr/p1P1pppp/b7/4q3/7n/8/PP1PPPPP/RNBQKBNR w KQk -", "c7c8q", -100),
    ("4kbnr/p1P1pppp/b7/4q3/7n/8/PPQPPPPP/RNB1KBNR w KQk -", "c7c8q", 200),
    ("4kbnr/p1P1pppp/b7/4q3/7n/8/PPQPPPPP/RNB1KBNR w KQk -", "c7c8q", 200),
    ("4kbnr/p1P4p/b1q5/5pP1/4n3/5Q2/PP1PPP1P/RNB1KBNR w KQk f6", "g5f6", 0),
    ("4kbnr/p1P4p/b1q5/5pP1/4n3/5Q2/PP1PPP1P/RNB1KBNR w KQk f6", "g5f6", 0),
    ("4kbnr/p1P4p/b1q5/5pP1/4n2Q/8/PP1PPP1P/RNB1KBNR w KQk f6", "g5f6", 0),
    ("1n2kb1r/p1P4p/2qb4/5pP1/4n2Q/8/PP1PPP1P/RNB1KBNR w KQk -", "c7b8q", 200),
    ("rnbqk2r/pp3ppp/2p1pn2/3p4/3P4/N1P1BN2/PPB1PPPb/R2Q1RK1 w kq -", "g1h2", 300),
    ("3N4/2K5/2n5/1k6/8/8/8/8 b - -", "c6d8", 0),
    ("3N4/2P5/2n5/1k6/8/8/8/4K3 b - -", "c6d8", -800),
    ("3n3r/2P5/8/1k6/8/8/3Q4/4K3 w - -", "d2d8", 300),
    ("3n3r/2P5/8/1k6/8/8/3Q4/4K3 w - -", "c7d8q", 700),
    ("r2n3r/2P1P3/4N3/1k6/8/8/8/4K3 w - -", "e6d8", 300),
    ("8/8/8/1k6/6b1/4N3/2p3K1/3n4 w - -", "e3d1", 0),
    ("8/8/1k6/8/8/2N1N3/4p1K1/3n4 w - -", "c3d1", 100),
    ("r1bqk1nr/pppp1ppp/2n5/1B2p3/1b2P3/5N2/PPPP1PPP/RNBQK2R w KQkq -", "e1g1", 0),
]


def find_default_engine() -> Path:
    repo_root = Path(__file__).resolve().parent.parent
    candidates = [
        repo_root / "build" / "nox_engine",
        repo_root / "nox_engine",
    ]
    for c in candidates:
        if c.is_file():
            return c
    return candidates[0]


def run_see(engine: Path, fen: str, move: str) -> int:
    cmds = f"see {move} {fen}\nquit\n"
    proc = subprocess.run(
        [str(engine)], input=cmds, capture_output=True, text=True, timeout=30
    )
    lines = [l.strip() for l in proc.stdout.splitlines() if l.strip()]
    if not lines:
        raise RuntimeError(f"engine produced no output for see {move} {fen}. stderr:\n{proc.stderr}")
    try:
        return int(lines[-1])
    except ValueError:
        raise RuntimeError(f"could not parse SEE value from output: {lines[-1]!r}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--engine", type=Path, default=None, help="path to the built nox_engine binary")
    args = ap.parse_args()

    engine = args.engine or find_default_engine()
    if not engine.is_file():
        print(f"ERROR: engine binary not found at {engine}")
        print("Build it first, e.g.:  bash compile_engine.sh")
        sys.exit(1)

    print(f"engine: {engine}")
    print(f"cases:  {len(SEE_CASES)}\n")

    failed = 0
    for fen, move, expected in SEE_CASES:
        try:
            actual = run_see(engine, fen, move)
        except Exception as e:
            print(f"[ERROR] {move:6s} {fen}: {e}")
            failed += 1
            continue
        if actual == expected:
            print(f"[ OK ]  {move:6s} see={actual:>6d}   {fen}")
        else:
            print(f"[FAIL]  {move:6s} see={actual:>6d} expected={expected:>6d}   {fen}")
            failed += 1

    total = len(SEE_CASES)
    print(f"\n{total - failed}/{total} SEE checks passed")
    if failed:
        print("Static exchange evaluation is incorrect -- do not rely on SEE-based pruning/ordering until this is green.")
        sys.exit(1)
    else:
        print("Static exchange evaluation looks correct.")
        sys.exit(0)


if __name__ == "__main__":
    main()