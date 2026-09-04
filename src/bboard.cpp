#include "bboard.h"
#include <sstream>
#include <cctype>
#include <cassert>
#include "zobrist.h"

namespace eng {

int BBoard::pieceIndexOf(char c) {
    switch (c) {
        case 'P': return WP; case 'N': return WN; case 'B': return WB;
        case 'R': return WR; case 'Q': return WQ; case 'K': return WK;
        case 'p': return BP; case 'n': return BN; case 'b': return BB;
        case 'r': return BR; case 'q': return BQ; case 'k': return BK;
        default:  return -1;
    }
}

char BBoard::charOfPieceIndex(int idx) {
    static const char CHARS[12] = {'P','N','B','R','Q','K','p','n','b','r','q','k'};
    if (idx < 0 || idx >= 12) return '.';
    return CHARS[idx];
}

void BBoard::recomputeOcc() {
    Bitboard w = 0, b = 0;
    for (int i = WP; i <= WK; ++i) w |= st.pieces[i];
    for (int i = BP; i <= BK; ++i) b |= st.pieces[i];
    st.occWhite = w;
    st.occBlack = b;
    st.occAll = w | b;
}

uint64_t BBoard::recomputeKeyFromScratch() const {
    uint64_t h = 0;
    for (int idx = 0; idx < 12; ++idx) {
        Bitboard bb = st.pieces[idx];
        while (bb) {
            int sq = popLsb(bb);
            h ^= Zobrist::piece[idx][sq];
        }
    }
    h ^= Zobrist::castling[st.castling & 15];
    if (st.ep != -1) h ^= Zobrist::epFile[st.ep % 8];
    if (st.side == 'b') h ^= Zobrist::side;
    return h;
}

void BBoard::setStartPos() {
    setFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void BBoard::setFEN(const std::string& fen) {
    std::istringstream ss(fen);
    std::string board_f, side_f, castling_f, ep_f;
    int half = 0, full = 1;
    ss >> board_f >> side_f >> castling_f >> ep_f >> half >> full;

    st.pieces.fill(0);
    int r = 7, f = 0;
    for (char ch : board_f) {
        if (ch == '/') { r--; f = 0; continue; }
        if (std::isdigit((unsigned char)ch)) { f += ch - '0'; continue; }
        int sq = r * 8 + f;
        int idx = pieceIndexOf(ch);
        if (idx >= 0) st.pieces[idx] |= sqBit(sq);
        f++;
    }
    recomputeOcc();

    st.side = side_f.empty() ? 'w' : side_f[0];

    int cr = 0;
    if (castling_f.find('K') != std::string::npos) cr |= 1;
    if (castling_f.find('Q') != std::string::npos) cr |= 2;
    if (castling_f.find('k') != std::string::npos) cr |= 4;
    if (castling_f.find('q') != std::string::npos) cr |= 8;
    st.castling = cr;

    st.ep = (ep_f.empty() || ep_f == "-") ? -1 : coordToSq(ep_f);
    st.halfmove = half;
    st.fullmove = full;

    st.wk = st.pieces[WK] ? lsbIndex(st.pieces[WK]) : -1;
    st.bk = st.pieces[BK] ? lsbIndex(st.pieces[BK]) : -1;

    st.zobristKey = recomputeKeyFromScratch();
}

std::string BBoard::getFEN() const {
    // Rebuild a flat 64-char occupancy view from the bitboards purely for
    // string formatting -- this is the one place it's convenient to think
    // in "one char per square" terms, matching the mailbox Board's getFEN()
    // output format exactly so both representations are interchangeable at
    // the UCI/FEN boundary.
    std::array<char, 64> flat;
    flat.fill('.');
    for (int idx = 0; idx < 12; ++idx) {
        Bitboard bb = st.pieces[idx];
        while (bb) {
            int sq = popLsb(bb);
            flat[sq] = charOfPieceIndex(idx);
        }
    }

    std::string rows;
    for (int r = 7; r >= 0; --r) {
        int empty = 0; std::string row;
        for (int f = 0; f < 8; ++f) {
            char p = flat[r * 8 + f];
            if (p == '.') { empty++; }
            else { if (empty) { row += std::to_string(empty); empty = 0; } row += p; }
        }
        if (empty) row += std::to_string(empty);
        rows += row; if (r) rows += '/';
    }

    std::string cr;
    if (st.castling & 1) cr += 'K';
    if (st.castling & 2) cr += 'Q';
    if (st.castling & 4) cr += 'k';
    if (st.castling & 8) cr += 'q';
    if (cr.empty()) cr = "-";

    std::string ep = (st.ep == -1) ? "-" : sqToCoord(st.ep);
    std::ostringstream out;
    out << rows << ' ' << st.side << ' ' << cr << ' ' << ep << ' ' << st.halfmove << ' ' << st.fullmove;
    return out.str();
}

// ---------------------------------------------------------------------------
// Check detection.
//
// "Is `sq` attacked by color `bySide`" is computed via the standard reversed-
// attack trick rather than scanning outward from `sq` in every direction the
// way the mailbox Board's squareAttacked() does: for a non-sliding piece
// type, the set of squares from which a piece of that type could attack `sq`
// is identical to the set of squares that piece would attack if it stood ON
// `sq` itself (attacks are symmetric for knights/kings, and for pawns you
// just look from the opposite color's attack table -- see the comment on
// the pawn check below). For sliders, the equivalent trick is: compute this
// square's own rook/bishop attack rays given the actual occupancy, and check
// whether an enemy rook/queen or bishop/queen sits on one of those rays.
// Either way, this is a handful of table lookups instead of walking rays
// outward by hand, and it reuses the exact same magic-bitboard tables
// Phase 1 already validated with 640,000 random-occupancy checks.
// ---------------------------------------------------------------------------
bool BBoard::squareAttacked(int sq, char bySide) const {
    bool byWhite = (bySide == 'w');

    // Pawns: PAWN_ATTACKS[color][s] is "the squares a pawn of `color`
    // standing on s attacks". A square `sq` is attacked by a pawn of color C
    // exactly when a pawn of color C sits on one of the squares that a pawn
    // of the OPPOSITE color standing on `sq` would attack -- i.e.
    // attackers-of-sq-by-C = PAWN_ATTACKS[!C][sq] & pawnsOfColor(C).
    Bitboard pawns = byWhite ? st.pieces[WP] : st.pieces[BP];
    int oppColorIdx = byWhite ? 1 : 0; // PAWN_ATTACKS index: 0=white,1=black
    if (PAWN_ATTACKS[oppColorIdx][sq] & pawns) return true;

    Bitboard knights = byWhite ? st.pieces[WN] : st.pieces[BN];
    if (KNIGHT_ATTACKS[sq] & knights) return true;

    Bitboard king = byWhite ? st.pieces[WK] : st.pieces[BK];
    if (KING_ATTACKS[sq] & king) return true;

    Bitboard bishopsQueens = byWhite ? (st.pieces[WB] | st.pieces[WQ]) : (st.pieces[BB] | st.pieces[BQ]);
    if (bishopAttacks(sq, st.occAll) & bishopsQueens) return true;

    Bitboard rooksQueens = byWhite ? (st.pieces[WR] | st.pieces[WQ]) : (st.pieces[BR] | st.pieces[BQ]);
    if (rookAttacks(sq, st.occAll) & rooksQueens) return true;

    return false;
}

// ---------------------------------------------------------------------------
// Pseudo-legal move generation.
// ---------------------------------------------------------------------------

void BBoard::genPawnMoves(std::vector<Move>& out) const {
    bool white = (st.side == 'w');
    Bitboard pawns = white ? st.pieces[WP] : st.pieces[BP];
    Bitboard empty = ~st.occAll;
    Bitboard enemyOcc = white ? st.occBlack : st.occWhite;
    int dir = white ? 8 : -8;
    int startRank = white ? 1 : 6;
    int promoRank = white ? 7 : 0;
    int colorIdx = white ? 0 : 1;

    Bitboard bb = pawns;
    while (bb) {
        int s = popLsb(bb);

        int to = s + dir;
        if (to >= 0 && to < 64 && (empty & sqBit(to))) {
            if (rankOfSq(to) == promoRank) {
                for (char pr : std::string("QRBN"))
                    out.push_back({s, to, white ? pr : static_cast<char>(std::tolower(pr)), PROMOTION});
            } else {
                out.push_back({s, to, 0, 0});
            }
            if (rankOfSq(s) == startRank) {
                int two = s + 2 * dir;
                if (empty & sqBit(two)) out.push_back({s, two, 0, DOUBLE_PAWN});
            }
        }

        Bitboard captures = PAWN_ATTACKS[colorIdx][s] & enemyOcc;
        while (captures) {
            int t = popLsb(captures);
            if (rankOfSq(t) == promoRank) {
                for (char pr : std::string("QRBN"))
                    out.push_back({s, t, white ? pr : static_cast<char>(std::tolower(pr)), static_cast<uint16_t>(PROMOTION | CAPTURE)});
            } else {
                out.push_back({s, t, 0, CAPTURE});
            }
        }

        if (st.ep != -1 && (PAWN_ATTACKS[colorIdx][s] & sqBit(st.ep))) {
            out.push_back({s, st.ep, 0, static_cast<uint16_t>(EN_PASSANT | CAPTURE)});
        }
    }
}

void BBoard::genKnightMoves(std::vector<Move>& out) const {
    bool white = (st.side == 'w');
    Bitboard knights = white ? st.pieces[WN] : st.pieces[BN];
    Bitboard ownOcc = white ? st.occWhite : st.occBlack;
    Bitboard bb = knights;
    while (bb) {
        int s = popLsb(bb);
        Bitboard targets = KNIGHT_ATTACKS[s] & ~ownOcc;
        while (targets) {
            int to = popLsb(targets);
            bool isCap = (st.occAll & sqBit(to)) != 0;
            out.push_back({s, to, 0, static_cast<uint16_t>(isCap ? CAPTURE : 0)});
        }
    }
}

void BBoard::genBishopMoves(std::vector<Move>& out) const {
    bool white = (st.side == 'w');
    Bitboard bishops = white ? st.pieces[WB] : st.pieces[BB];
    Bitboard ownOcc = white ? st.occWhite : st.occBlack;
    Bitboard bb = bishops;
    while (bb) {
        int s = popLsb(bb);
        Bitboard targets = bishopAttacks(s, st.occAll) & ~ownOcc;
        while (targets) {
            int to = popLsb(targets);
            bool isCap = (st.occAll & sqBit(to)) != 0;
            out.push_back({s, to, 0, static_cast<uint16_t>(isCap ? CAPTURE : 0)});
        }
    }
}

void BBoard::genRookMoves(std::vector<Move>& out) const {
    bool white = (st.side == 'w');
    Bitboard rooks = white ? st.pieces[WR] : st.pieces[BR];
    Bitboard ownOcc = white ? st.occWhite : st.occBlack;
    Bitboard bb = rooks;
    while (bb) {
        int s = popLsb(bb);
        Bitboard targets = rookAttacks(s, st.occAll) & ~ownOcc;
        while (targets) {
            int to = popLsb(targets);
            bool isCap = (st.occAll & sqBit(to)) != 0;
            out.push_back({s, to, 0, static_cast<uint16_t>(isCap ? CAPTURE : 0)});
        }
    }
}

void BBoard::genQueenMoves(std::vector<Move>& out) const {
    bool white = (st.side == 'w');
    Bitboard queens = white ? st.pieces[WQ] : st.pieces[BQ];
    Bitboard ownOcc = white ? st.occWhite : st.occBlack;
    Bitboard bb = queens;
    while (bb) {
        int s = popLsb(bb);
        Bitboard targets = queenAttacks(s, st.occAll) & ~ownOcc;
        while (targets) {
            int to = popLsb(targets);
            bool isCap = (st.occAll & sqBit(to)) != 0;
            out.push_back({s, to, 0, static_cast<uint16_t>(isCap ? CAPTURE : 0)});
        }
    }
}

void BBoard::genKingMoves(std::vector<Move>& out) const {
    bool white = (st.side == 'w');
    Bitboard kingBB = white ? st.pieces[WK] : st.pieces[BK];
    if (!kingBB) return; // defensive: shouldn't happen in a legal position
    int s = lsbIndex(kingBB);
    Bitboard ownOcc = white ? st.occWhite : st.occBlack;
    Bitboard targets = KING_ATTACKS[s] & ~ownOcc;
    while (targets) {
        int to = popLsb(targets);
        bool isCap = (st.occAll & sqBit(to)) != 0;
        out.push_back({s, to, 0, static_cast<uint16_t>(isCap ? CAPTURE : 0)});
    }
    genCastleMoves(out);
}

// Mirrors the mailbox Board's genCastles() exactly -- same square numbers
// (4/5/6 = e1/f1/g1, 0/1/2/3 = a1/b1/c1/d1, 60/61/62 = e8/f8/g8, 56/57/58/59
// = a8/b8/c8/d8), same three-square "king's start, transit, destination
// must not be attacked" rule for the king's own path, and same "the b-file
// square the rook (but not the king) passes through queenside just needs to
// be empty, not unattacked" rule.
void BBoard::genCastleMoves(std::vector<Move>& out) const {
    bool white = (st.side == 'w');
    if (white) {
        if ((st.castling & 1) && !(st.occAll & (sqBit(5) | sqBit(6)))) {
            if (!squareAttacked(4, 'b') && !squareAttacked(5, 'b') && !squareAttacked(6, 'b'))
                out.push_back({4, 6, 0, CASTLE});
        }
        if ((st.castling & 2) && !(st.occAll & (sqBit(1) | sqBit(2) | sqBit(3)))) {
            if (!squareAttacked(4, 'b') && !squareAttacked(3, 'b') && !squareAttacked(2, 'b'))
                out.push_back({4, 2, 0, CASTLE});
        }
    } else {
        if ((st.castling & 4) && !(st.occAll & (sqBit(61) | sqBit(62)))) {
            if (!squareAttacked(60, 'w') && !squareAttacked(61, 'w') && !squareAttacked(62, 'w'))
                out.push_back({60, 62, 0, CASTLE});
        }
        if ((st.castling & 8) && !(st.occAll & (sqBit(57) | sqBit(58) | sqBit(59)))) {
            if (!squareAttacked(60, 'w') && !squareAttacked(59, 'w') && !squareAttacked(58, 'w'))
                out.push_back({60, 58, 0, CASTLE});
        }
    }
}

std::vector<Move> BBoard::generatePseudoLegalMoves() const {
    std::vector<Move> out;
    out.reserve(64);
    genPawnMoves(out);
    genKnightMoves(out);
    genBishopMoves(out);
    genRookMoves(out);
    genQueenMoves(out);
    genKingMoves(out);
    return out;
}

// ---------------------------------------------------------------------------
// makeMove / unmakeMove.
//
// makeMove() incrementally updates st.zobristKey via XOR in/out as each
// piece/castling/ep/side change is applied -- never a full rescan. Undo is
// handled separately and much more simply: a full BBState snapshot is taken
// up front and restored wholesale in unmakeMove(), so there's no parallel
// "manually reverse every mutation makeMove() just made" logic to keep in
// sync as this function evolves.
// ---------------------------------------------------------------------------

bool BBoard::makeMove(const Move& m) {
    BBUndo u;
    u.prevState = st;
    stack.push_back(u);

    bool white = (st.side == 'w');

    int fromIdx = -1;
    for (int i = 0; i < 12; ++i) {
        if (st.pieces[i] & sqBit(m.from)) { fromIdx = i; break; }
    }

    int capSq = m.to;
    if (m.flags & EN_PASSANT) {
        capSq = white ? m.to - 8 : m.to + 8;
    }
    int capturedIdx = -1;
    for (int i = 0; i < 12; ++i) {
        if (st.pieces[i] & sqBit(capSq)) { capturedIdx = i; break; }
    }

    bool isPawnMove = (fromIdx == WP || fromIdx == BP);
    if (isPawnMove || capturedIdx != -1) st.halfmove = 0;
    else st.halfmove++;

    // Remove the old ep/castling Zobrist contributions before mutating them
    // -- they'll be added back (with whatever the new values turn out to be)
    // once castling rights and the new ep square are settled below.
    if (st.ep != -1) st.zobristKey ^= Zobrist::epFile[st.ep % 8];
    st.zobristKey ^= Zobrist::castling[st.castling & 15];
    st.ep = -1;

    if (capturedIdx != -1) {
        st.pieces[capturedIdx] &= ~sqBit(capSq);
        st.zobristKey ^= Zobrist::piece[capturedIdx][capSq];
    }

    st.pieces[fromIdx] &= ~sqBit(m.from);
    st.zobristKey ^= Zobrist::piece[fromIdx][m.from];

    int placedIdx = (m.flags & PROMOTION) ? pieceIndexOf(m.promo) : fromIdx;
    st.pieces[placedIdx] |= sqBit(m.to);
    st.zobristKey ^= Zobrist::piece[placedIdx][m.to];

    if (m.flags & CASTLE) {
        if (placedIdx == WK) {
            if (m.to == 6) {
                st.pieces[WR] &= ~sqBit(7); st.pieces[WR] |= sqBit(5);
                st.zobristKey ^= Zobrist::piece[WR][7]; st.zobristKey ^= Zobrist::piece[WR][5];
            } else {
                st.pieces[WR] &= ~sqBit(0); st.pieces[WR] |= sqBit(3);
                st.zobristKey ^= Zobrist::piece[WR][0]; st.zobristKey ^= Zobrist::piece[WR][3];
            }
            st.wk = m.to;
        } else if (placedIdx == BK) {
            if (m.to == 62) {
                st.pieces[BR] &= ~sqBit(63); st.pieces[BR] |= sqBit(61);
                st.zobristKey ^= Zobrist::piece[BR][63]; st.zobristKey ^= Zobrist::piece[BR][61];
            } else {
                st.pieces[BR] &= ~sqBit(56); st.pieces[BR] |= sqBit(59);
                st.zobristKey ^= Zobrist::piece[BR][56]; st.zobristKey ^= Zobrist::piece[BR][59];
            }
            st.bk = m.to;
        }
    } else {
        if (placedIdx == WK) st.wk = m.to;
        else if (placedIdx == BK) st.bk = m.to;
    }

    if (m.flags & DOUBLE_PAWN) {
        st.ep = white ? m.from + 8 : m.from - 8;
    }

    // Castling rights: lost permanently the moment the king moves at all, or
    // a rook moves off (or is captured on) its original corner square --
    // same square numbers and semantics as the mailbox Board.
    if (fromIdx == WK) st.castling &= ~(1 | 2);
    if (fromIdx == WR) { if (m.from == 0) st.castling &= ~2; if (m.from == 7) st.castling &= ~1; }
    if (capturedIdx == WR) { if (capSq == 0) st.castling &= ~2; if (capSq == 7) st.castling &= ~1; }
    if (fromIdx == BK) st.castling &= ~(4 | 8);
    if (fromIdx == BR) { if (m.from == 56) st.castling &= ~8; if (m.from == 63) st.castling &= ~4; }
    if (capturedIdx == BR) { if (capSq == 56) st.castling &= ~8; if (capSq == 63) st.castling &= ~4; }

    st.zobristKey ^= Zobrist::castling[st.castling & 15];
    if (st.ep != -1) st.zobristKey ^= Zobrist::epFile[st.ep % 8];

    if (!white) st.fullmove++;
    st.side = white ? 'b' : 'w';
    st.zobristKey ^= Zobrist::side;

    recomputeOcc();

    int ownKingSq = white ? st.wk : st.bk;
    char enemySide = white ? 'b' : 'w';
    if (squareAttacked(ownKingSq, enemySide)) {
        unmakeMove();
        return false;
    }
    return true;
}

void BBoard::unmakeMove() {
    assert(!stack.empty() && "unmakeMove() called with an empty undo stack");
    st = stack.back().prevState;
    stack.pop_back();
}

std::vector<Move> BBoard::generateLegalMoves() {
    std::vector<Move> pseudo = generatePseudoLegalMoves();
    std::vector<Move> legal;
    legal.reserve(pseudo.size());
    for (const auto& m : pseudo) {
        if (makeMove(m)) {
            legal.push_back(m);
            unmakeMove();
        }
    }
    return legal;
}

}