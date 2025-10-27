#!/usr/bin/env python3
import argparse, subprocess, shlex

# Uses engine's UCI evalfen command to produce training pairs: "FEN ; score"

def uci_evalfen(engine_path, fen, evalfile=None, use_nnue=False):
    cmds = [
        "uci\n",
        "isready\n",
    ]
    if evalfile:
        cmds.append(f"setoption name EvalFile value {evalfile}\n")
    if use_nnue:
        cmds.append("setoption name Use NNUE value true\n")
    else:
        cmds.append("setoption name Use NNUE value false\n")
    cmds.append(f"evalfen {fen}\n")
    cmds.append("quit\n")
    p = subprocess.run([engine_path], input="".join(cmds).encode(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out = p.stdout.decode(errors='ignore').strip().splitlines()
    # last non-empty line should be the number
    for line in reversed(out):
        line = line.strip()
        if not line:
            continue
        try:
            return float(line)
        except:
            pass
    raise RuntimeError("Failed to parse engine output")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--engine', required=True, help='path to built engine executable (nox_engine)')
    ap.add_argument('--fens', required=True, help='input FEN list (one per line)')
    ap.add_argument('--out', required=True, help='output file (FEN ; score_cp)')
    ap.add_argument('--evalfile', help='NOXNET .nox path (optional)')
    ap.add_argument('--use-nnue', action='store_true', help='use NNUE path (NOXNET) if set')
    ap.add_argument('--limit', type=int, default=0, help='limit number of FENs (0 = all)')
    args = ap.parse_args()

    cnt = 0
    with open(args.fens, 'r') as f_in, open(args.out, 'w') as f_out:
        for line in f_in:
            fen = line.strip()
            if not fen:
                continue
            try:
                sc = uci_evalfen(args.engine, fen, args.evalfile, args.use_nnue)
            except Exception as e:
                continue
            f_out.write(f"{fen} ; {int(sc)}\n")
            cnt += 1
            if args.limit and cnt >= args.limit:
                break
    print(f"wrote {args.out} ({cnt} examples)")

if __name__ == '__main__':
    main()
