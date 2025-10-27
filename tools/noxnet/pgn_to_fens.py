#!/usr/bin/env python3
import argparse
import chess.pgn

# Extract FENs from PGN at a given stride (every N plies),
# optionally limited to a max number per game.

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('pgn', help='input PGN file')
    ap.add_argument('out', help='output FEN list file')
    ap.add_argument('--stride', type=int, default=4, help='take every N plies')
    ap.add_argument('--max-per-game', type=int, default=64, help='limit per game')
    args = ap.parse_args()

    taken = 0
    with open(args.pgn, 'r', encoding='utf-8', errors='ignore') as f, open(args.out, 'w') as out:
        while True:
            game = chess.pgn.read_game(f)
            if game is None:
                break
            board = game.board()
            k = 0
            for i, move in enumerate(game.mainline_moves(), 1):
                board.push(move)
                if i % args.stride == 0:
                    out.write(board.fen() + '\n')
                    k += 1
                    if k >= args.max_per_game:
                        break

if __name__ == '__main__':
    main()
