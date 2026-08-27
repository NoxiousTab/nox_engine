#!/usr/bin/env python3
"""
perft_test.py -- move-generation regression test for nox_engine.

Runs the engine's UCI `perft` command against a set of standard, publicly
known-correct chess perft positions (the same positions used across the
chess engine community to validate move generators) and compares the node
counts the engine reports against the known-correct values.

This does NOT require any external chess library -- the expected values
below are fixed reference numbers, not computed on the fly.

Usage:
    python3 tests/perft_test.py                  # "full" suite (default)
    python3 tests/perft_test.py --mode quick      # fast sanity check (<1s)
    python3 tests/perft_test.py --mode deep       # adds slower, deeper checks
    python3 tests/perft_test.py --engine PATH     # point at a specific binary

Exit code is 0 if every check passes, 1 otherwise, so this is safe to use
in a pre-commit hook or CI later if you want.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

# Each position: (name, fen_or_None_for_startpos, [(depth, expected_nodes), ...])
# Depths are grouped by how expensive they are; --mode picks which depths run.
# Source: these are the standard "Perft Results" reference positions
# (position 1 = startpos, positions 2-6 as commonly labeled in engine test suites).
POSITIONS = [
    (
        "startpos",
        None,
        {
            "quick": [(4, 197281)],
            "full":  [(5, 4865609)],
            "deep":  [(6, 119060324)],
        },
    ),
    (
        "kiwipete",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        {
            "quick": [(3, 97862)],
            "full":  [(4, 4085603)],
            "deep":  [(5, 193690690)],
        },
    ),
    (
        "position3",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        {
            "quick": [(5, 674624)],
            "full":  [(6, 11030083)],
            "deep":  [(6, 11030083)],  # depth 7 (178633661) is very slow; skip by default
        },
    ),
    (
        "position4",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        {
            "quick": [(4, 422333)],
            "full":  [(5, 15833292)],
            "deep":  [(5, 15833292)],  # depth 6 (706045033) is very slow; skip by default
        },
    ),
    (
        "position5",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        {
            "quick": [(3, 62379)],
            "full":  [(4, 2103487)],
            "deep":  [(5, 89941194)],
        },
    ),
    (
        "position6",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
        {
            "quick": [(3, 89890)],
            "full":  [(4, 3894594)],
            "deep":  [(5, 164075551)],
        },
    ),
]


def find_default_engine() -> Path:
    """Look for the built engine relative to this script's location."""
    repo_root = Path(__file__).resolve().parent.parent
    candidates = [
        repo_root / "build" / "nox_engine",
        repo_root / "nox_engine",
    ]
    for c in candidates:
        if c.is_file():
            return c
    # fall back to the first candidate even if missing, so the error
    # message below can tell the user exactly what path we tried
    return candidates[0]


def run_perft(engine: Path, fen: str | None, depth: int) -> tuple[int, float]:
    if fen is None:
        pos_cmd = "position startpos"
    else:
        pos_cmd = f"position fen {fen}"
    cmds = f"{pos_cmd}\nperft {depth}\nquit\n"
    start = time.time()
    proc = subprocess.run(
        [str(engine)],
        input=cmds,
        capture_output=True,
        text=True,
        timeout=600,
    )
    elapsed = time.time() - start
    lines = [l.strip() for l in proc.stdout.splitlines() if l.strip()]
    if not lines:
        raise RuntimeError(
            f"engine produced no output for '{pos_cmd}' / perft {depth}. "
            f"stderr:\n{proc.stderr}"
        )
    last = lines[-1]
    try:
        nodes = int(last)
    except ValueError:
        raise RuntimeError(
            f"could not parse node count from engine output: {last!r} "
            f"(full stdout: {lines})"
        )
    return nodes, elapsed


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument(
        "--engine",
        type=Path,
        default=None,
        help="path to the built nox_engine binary (default: auto-detect build/nox_engine)",
    )
    ap.add_argument(
        "--mode",
        choices=["quick", "full", "deep"],
        default="full",
        help="how thorough to be: quick (<1s), full (default, ~1-2 min), deep (several minutes)",
    )
    args = ap.parse_args()

    engine = args.engine or find_default_engine()
    if not engine.is_file():
        print(f"ERROR: engine binary not found at {engine}")
        print("Build it first, e.g.:  bash compile_engine.sh")
        print("Or point at it explicitly:  python3 tests/perft_test.py --engine /path/to/nox_engine")
        sys.exit(1)

    print(f"engine: {engine}")
    print(f"mode:   {args.mode}\n")

    total = 0
    failed = 0
    total_time = 0.0

    for name, fen, depths_by_mode in POSITIONS:
        for depth, expected in depths_by_mode[args.mode]:
            total += 1
            label = f"{name:10s} depth {depth}"
            try:
                nodes, elapsed = run_perft(engine, fen, depth)
            except Exception as e:
                print(f"[ERROR] {label}: {e}")
                failed += 1
                continue
            total_time += elapsed
            if nodes == expected:
                print(f"[ OK ]  {label}: {nodes} nodes ({elapsed:.2f}s)")
            else:
                diff = nodes - expected
                print(
                    f"[FAIL]  {label}: got {nodes}, expected {expected} "
                    f"(diff {diff:+d}) ({elapsed:.2f}s)"
                )
                failed += 1

    print(f"\n{total - failed}/{total} checks passed, total time {total_time:.2f}s")
    if failed:
        print("Move generation has a correctness bug -- do not trust search results until this is green.")
        sys.exit(1)
    else:
        print("Move generation looks correct.")
        sys.exit(0)


if __name__ == "__main__":
    main()

