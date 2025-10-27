import torch
from torch.utils.data import Dataset

# Features must match C++ build_features in src/nnue.cpp
# Layout (size 782):
# [0..767): 12*64 one-hot piece-square, side-relative (flip both rank/file if black to move)
# [768]: side to move (1 if white else 0)
# [769..772]: castling K,Q,k,q flags
# [773..780]: en-passant file one-hot, or all zeros if none
# [781]: phase scalar in [0,1]

PIECE_TO_IDX = {
    'P': 0, 'N': 1, 'B': 2, 'R': 3, 'Q': 4, 'K': 5,
    'p': 6, 'n': 7, 'b': 8, 'r': 9, 'q': 10, 'k': 11,
}

def orient_sq(sq_idx: int, side: str) -> int:
    if side == 'w':
        return sq_idx
    return 63 - sq_idx  # flip both rank and file


def fen_to_board_array(fen: str):
    # returns list length 64 of piece chars or '.'
    board = []
    parts = fen.split()
    rows = parts[0].split('/')
    for r in rows:
        for ch in r:
            if ch.isdigit():
                board += ['.'] * int(ch)
            else:
                board.append(ch)
    return board, parts


def features_from_fen(fen: str):
    x = [0.0] * 782
    board, parts = fen_to_board_array(fen)
    side = parts[1]
    castling = parts[2] if len(parts) > 2 else '-'
    ep = parts[3] if len(parts) > 3 else '-'

    # pieces
    for idx, p in enumerate(board):
        if p == '.':
            continue
        pi = PIECE_TO_IDX.get(p, -1)
        if pi < 0:
            continue
        osq = orient_sq(idx, side)
        onehot = pi * 64 + osq
        if 0 <= onehot < 768:
            x[onehot] = 1.0

    # side to move
    x[768] = 1.0 if side == 'w' else 0.0

    # castling
    if 'K' in castling: x[769] = 1.0
    if 'Q' in castling: x[770] = 1.0
    if 'k' in castling: x[771] = 1.0
    if 'q' in castling: x[772] = 1.0

    # en passant file
    if ep != '-' and len(ep) >= 1 and ep[0] in 'abcdefgh':
        file_idx = ord(ep[0]) - ord('a')
        x[773 + file_idx] = 1.0

    # phase scalar (coarse material metric)
    def abs_val(p):
        return 1 if p in 'Pp' else 3 if p in 'NnBb' else 5 if p in 'Rr' else 9 if p in 'Qq' else 0
    total = sum(abs_val(p) for p in board)
    phase = min(1.0, total / 78.0)
    x[781] = phase

    return torch.tensor(x, dtype=torch.float32)


class FenScoreDataset(Dataset):
    """
    Expects a text file with lines: FEN ; score_cp
    Example:
    rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 ; 0
    """
    def __init__(self, path):
        self.items = []
        with open(path, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                if ';' in line:
                    fen, sc = line.split(';', 1)
                    fen = fen.strip()
                    try:
                        score = float(sc)
                    except Exception:
                        continue
                    self.items.append((fen, score))

    def __len__(self):
        return len(self.items)

    def __getitem__(self, idx):
        fen, score = self.items[idx]
        x = features_from_fen(fen)
        y = torch.tensor(score, dtype=torch.float32)
        return x, y
