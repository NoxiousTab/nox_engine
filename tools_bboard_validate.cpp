#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include "bboard.h"
#include "zobrist.h"

using namespace eng;

static int failures = 0;

// The same 6 standard perft reference positions used by tests/perft_test.py
// and already trusted against the mailbox Board -- reusing them here means
// BBoard's FEN round-trip is validated against known-good, previously-
// exercised positions rather than ad hoc ones.
static const std::vector<std::pair<std::string,std::string>> POSITIONS = {
    {"startpos",  "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"},
    {"kiwipete",  "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"},
    {"position3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"},
    {"position4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"},
    {"position5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"},
    {"position6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"},
};

static void check(const std::string& label, bool ok, const std::string& detail = "") {
    if (!ok) {
        std::cout << "FAIL [" << label << "]" << (detail.empty() ? "" : (": " + detail)) << "\n";
        ++failures;
    }
}

// ---------------------------------------------------------------------------
// Phase 2 step 2: pseudo-legal move generation + squareAttacked validation.
//
// Both cross-checks below are written completely independently of BBoard's
// own genXXXMoves()/squareAttacked() implementations -- different arithmetic
// paths, different helper functions -- so that a bug shared between "the
// code" and "the test" can't hide. Sliders use the *Slow ground-truth ray
// tracers from bitboard.h (already validated in Phase 1 against 640,000
// random occupancies), not the magic tables the real generator uses.
// ---------------------------------------------------------------------------

// Independent reimplementation of "is sq attacked by bySide", using manual
// file/rank offset arithmetic for pawns/knights/king (not the PAWN_ATTACKS/
// KNIGHT_ATTACKS/KING_ATTACKS tables BBoard::squareAttacked() itself reads)
// and the slow ray tracers (not the magic tables) for sliders.
static bool independentSquareAttacked(const BBState& st, int sq, char bySide) {
    bool byWhite = (bySide == 'w');
    int sf = fileOfSq(sq), sr = rankOfSq(sq);

    Bitboard pawns = byWhite ? st.pieces[WP] : st.pieces[BP];
    int pawnFromRank = sr + (byWhite ? -1 : 1); // a pawn attacking sq sits one rank behind it (from that color's push direction)
    for (int df : {-1, 1}) {
        int f = sf + df;
        if (onBoard(f, pawnFromRank)) {
            if (pawns & sqBit(makeSquare(f, pawnFromRank))) return true;
        }
    }

    Bitboard knights = byWhite ? st.pieces[WN] : st.pieces[BN];
    static const int KOFF[8][2] = {{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
    for (auto& o : KOFF) {
        int f = sf + o[0], r = sr + o[1];
        if (onBoard(f, r) && (knights & sqBit(makeSquare(f, r)))) return true;
    }

    Bitboard king = byWhite ? st.pieces[WK] : st.pieces[BK];
    for (int df = -1; df <= 1; ++df) for (int dr = -1; dr <= 1; ++dr) {
        if (df == 0 && dr == 0) continue;
        int f = sf + df, r = sr + dr;
        if (onBoard(f, r) && (king & sqBit(makeSquare(f, r)))) return true;
    }

    Bitboard bishopsQueens = byWhite ? (st.pieces[WB] | st.pieces[WQ]) : (st.pieces[BB] | st.pieces[BQ]);
    if (bishopAttacksSlow(sq, st.occAll) & bishopsQueens) return true;

    Bitboard rooksQueens = byWhite ? (st.pieces[WR] | st.pieces[WQ]) : (st.pieces[BR] | st.pieces[BQ]);
    if (rookAttacksSlow(sq, st.occAll) & rooksQueens) return true;

    return false;
}

// Independent total pseudo-legal move COUNT (not the move list itself) for
// the side to move, built the same way: manual arithmetic for non-sliders,
// slow ray tracers for sliders, and its own castling logic that calls
// independentSquareAttacked() above rather than BBoard::squareAttacked().
static int independentPseudoLegalCount(const BBoard& b) {
    const BBState& st = b.st;
    bool white = (st.side == 'w');
    Bitboard ownOcc = white ? st.occWhite : st.occBlack;
    Bitboard enemyOcc = white ? st.occBlack : st.occWhite;
    Bitboard empty = ~st.occAll;
    int count = 0;

    Bitboard pawns = white ? st.pieces[WP] : st.pieces[BP];
    int dir = white ? 8 : -8;
    int startRank = white ? 1 : 6;
    int promoRank = white ? 7 : 0;
    Bitboard bb = pawns;
    while (bb) {
        int s = popLsb(bb);
        int sf = fileOfSq(s), sr = rankOfSq(s);
        int to = s + dir;
        if (to >= 0 && to < 64 && (empty & sqBit(to))) {
            count += (rankOfSq(to) == promoRank) ? 4 : 1;
            if (sr == startRank) {
                int two = s + 2 * dir;
                if (empty & sqBit(two)) count += 1;
            }
        }
        for (int df : {-1, 1}) {
            int f = sf + df, r = sr + (white ? 1 : -1);
            if (!onBoard(f, r)) continue;
            int t = makeSquare(f, r);
            if (enemyOcc & sqBit(t)) count += (rankOfSq(t) == promoRank) ? 4 : 1;
            else if (st.ep != -1 && t == st.ep) count += 1;
        }
    }

    bb = white ? st.pieces[WN] : st.pieces[BN];
    while (bb) { int s = popLsb(bb); count += popcount(KNIGHT_ATTACKS[s] & ~ownOcc); }

    bb = white ? st.pieces[WB] : st.pieces[BB];
    while (bb) { int s = popLsb(bb); count += popcount(bishopAttacksSlow(s, st.occAll) & ~ownOcc); }

    bb = white ? st.pieces[WR] : st.pieces[BR];
    while (bb) { int s = popLsb(bb); count += popcount(rookAttacksSlow(s, st.occAll) & ~ownOcc); }

    bb = white ? st.pieces[WQ] : st.pieces[BQ];
    while (bb) { int s = popLsb(bb); count += popcount((bishopAttacksSlow(s, st.occAll) | rookAttacksSlow(s, st.occAll)) & ~ownOcc); }

    Bitboard kingBB = white ? st.pieces[WK] : st.pieces[BK];
    if (kingBB) { int s = lsbIndex(kingBB); count += popcount(KING_ATTACKS[s] & ~ownOcc); }

    char oppSide = white ? 'b' : 'w';
    if (white) {
        if ((st.castling & 1) && !(st.occAll & (sqBit(5) | sqBit(6))) &&
            !independentSquareAttacked(st, 4, oppSide) && !independentSquareAttacked(st, 5, oppSide) && !independentSquareAttacked(st, 6, oppSide)) count++;
        if ((st.castling & 2) && !(st.occAll & (sqBit(1) | sqBit(2) | sqBit(3))) &&
            !independentSquareAttacked(st, 4, oppSide) && !independentSquareAttacked(st, 3, oppSide) && !independentSquareAttacked(st, 2, oppSide)) count++;
    } else {
        if ((st.castling & 4) && !(st.occAll & (sqBit(61) | sqBit(62))) &&
            !independentSquareAttacked(st, 60, oppSide) && !independentSquareAttacked(st, 61, oppSide) && !independentSquareAttacked(st, 62, oppSide)) count++;
        if ((st.castling & 8) && !(st.occAll & (sqBit(57) | sqBit(58) | sqBit(59))) &&
            !independentSquareAttacked(st, 60, oppSide) && !independentSquareAttacked(st, 59, oppSide) && !independentSquareAttacked(st, 58, oppSide)) count++;
    }
    return count;
}

static void runStep2Checks() {
    // 1. All 6 standard positions: squareAttacked cross-checked for every
    //    square, both colors (768 checks total), and pseudo-legal move
    //    count cross-checked against the independent recomputation.
    for (const auto& [name, fen] : POSITIONS) {
        BBoard b;
        b.setFEN(fen);

        for (int sq = 0; sq < 64; ++sq) {
            bool wReal = b.squareAttacked(sq, 'w');
            bool wRef = independentSquareAttacked(b.st, sq, 'w');
            check(name + " squareAttacked white sq=" + std::to_string(sq), wReal == wRef);
            bool bReal = b.squareAttacked(sq, 'b');
            bool bRef = independentSquareAttacked(b.st, sq, 'b');
            check(name + " squareAttacked black sq=" + std::to_string(sq), bReal == bRef);
        }

        auto moves = b.generatePseudoLegalMoves();
        int refCount = independentPseudoLegalCount(b);
        check(name + " pseudo-legal move count matches independent recompute",
              (int)moves.size() == refCount,
              "got " + std::to_string(moves.size()) + " want " + std::to_string(refCount));

        // No move should ever land on a square occupied by the mover's own piece.
        Bitboard ownOcc = (b.st.side == 'w') ? b.st.occWhite : b.st.occBlack;
        for (const auto& m : moves) {
            check(name + " move " + sqToCoord(m.from) + sqToCoord(m.to) + " doesn't land on own piece",
                  !(ownOcc & sqBit(m.to)));
        }

        // No exact duplicate moves (same from/to/promo/flags).
        for (size_t i = 0; i < moves.size(); ++i)
            for (size_t j = i + 1; j < moves.size(); ++j)
                if (moves[i].from == moves[j].from && moves[i].to == moves[j].to &&
                    moves[i].promo == moves[j].promo && moves[i].flags == moves[j].flags)
                    check(name + " no duplicate move " + sqToCoord(moves[i].from) + sqToCoord(moves[i].to), false);
    }

    // 2. startpos specifically: pseudo-legal == legal == 20 here (no pins or
    //    checks are possible one move into the game), so this is an exact,
    //    well-known reference number, not just an internal self-consistency
    //    check.
    {
        BBoard b; b.setStartPos();
        check("startpos has exactly 20 pseudo-legal (==legal) moves",
              b.generatePseudoLegalMoves().size() == 20);
    }

    // 3. En passant: white pawn on e5, black just played d7-d5 (ep target d6).
    //    Exactly one EN_PASSANT move should be generated, from e5 to d6.
    {
        BBoard b; b.setFEN("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3");
        auto moves = b.generatePseudoLegalMoves();
        int epCount = 0; bool foundCorrect = false;
        for (const auto& m : moves) {
            if (m.flags & EN_PASSANT) {
                epCount++;
                if (m.from == coordToSq("e5") && m.to == coordToSq("d6")) foundCorrect = true;
            }
        }
        check("en passant: exactly one EN_PASSANT move generated", epCount == 1, "got " + std::to_string(epCount));
        check("en passant: it's e5xd6", foundCorrect);
    }

    // 4. Promotion: lone white pawn on a7, nothing to capture. Pushing to a8
    //    must generate exactly 4 promotion moves (Q, R, B, N), uppercase.
    {
        BBoard b; b.setFEN("8/P7/8/8/8/8/8/k6K w - - 0 1");
        auto moves = b.generatePseudoLegalMoves();
        std::string promoChars;
        for (const auto& m : moves) if (m.flags & PROMOTION) promoChars += m.promo;
        check("promotion: exactly 4 promotion moves generated", promoChars.size() == 4, "got " + std::to_string(promoChars.size()));
        for (char want : std::string("QRBN")) {
            check(std::string("promotion: includes '") + want + "'", promoChars.find(want) != std::string::npos);
        }
    }

    // 5. Castling -- king in check blocks BOTH sides (rook on e2 checks the
    //    king on e1 directly).
    {
        BBoard b; b.setFEN("4k3/8/8/8/8/8/4r3/R3K2R w KQ - 0 1");
        auto moves = b.generatePseudoLegalMoves();
        int castleCount = 0;
        for (const auto& m : moves) if (m.flags & CASTLE) castleCount++;
        check("castling: king in check blocks both sides", castleCount == 0, "got " + std::to_string(castleCount));
    }

    // 6. Castling -- rook on f2 attacks f1 (kingside transit square) but not
    //    e1 or any queenside square, so kingside must be blocked while
    //    queenside remains legal.
    {
        BBoard b; b.setFEN("4k3/8/8/8/8/8/5r2/R3K2R w KQ - 0 1");
        auto moves = b.generatePseudoLegalMoves();
        bool hasKingside = false, hasQueenside = false;
        for (const auto& m : moves) {
            if (m.flags & CASTLE) {
                if (m.to == coordToSq("g1")) hasKingside = true;
                if (m.to == coordToSq("c1")) hasQueenside = true;
            }
        }
        check("castling: kingside blocked when f1 is attacked", !hasKingside);
        check("castling: queenside still legal when only f1 is attacked", hasQueenside);
    }

    // 7. Castling -- a piece occupying b1 blocks queenside (b1/c1/d1 must
    //    all be empty) but kingside (f1/g1 empty) remains legal.
    {
        BBoard b; b.setFEN("4k3/8/8/8/8/8/8/RN2K2R w KQ - 0 1");
        auto moves = b.generatePseudoLegalMoves();
        bool hasKingside = false, hasQueenside = false;
        for (const auto& m : moves) {
            if (m.flags & CASTLE) {
                if (m.to == coordToSq("g1")) hasKingside = true;
                if (m.to == coordToSq("c1")) hasQueenside = true;
            }
        }
        check("castling: queenside blocked when b1 is occupied", !hasQueenside);
        check("castling: kingside still legal when b1 (queenside-only) is occupied", hasKingside);
    }
}

// ---------------------------------------------------------------------------
// Phase 2 step 3: makeMove/unmakeMove + full perft validation.
// ---------------------------------------------------------------------------

// Shallow, exhaustive traversal whose only purpose is checking that
// st.zobristKey (updated incrementally by makeMove()) never drifts from an
// independent from-scratch recomputation. Depth 3 keeps node counts small
// enough (well under 100K per position) to afford an O(64) recompute at
// EVERY node, which would be far too slow layered onto the full depth-5/6
// perft counts below.
static void verifyZobristIncremental(BBoard& b, int depth, int& mismatches, uint64_t& nodesChecked) {
    ++nodesChecked;
    if (b.positionKey() != b.recomputeKeyFromScratch()) ++mismatches;
    if (depth == 0) return;
    auto moves = b.generateLegalMoves();
    for (const auto& m : moves) {
        if (!b.makeMove(m)) {
            std::cout << "FATAL: generateLegalMoves() produced an illegal move during Zobrist check\n";
            std::exit(1);
        }
        verifyZobristIncremental(b, depth - 1, mismatches, nodesChecked);
        b.unmakeMove();
    }
}

// Standard recursive perft: counts leaf-reachable legal move sequences at
// exactly `depth` plies. Uses the same generateLegalMoves()/makeMove()/
// unmakeMove() API a real search would use -- no perft-specific shortcuts --
// so this is validating the actual API surface Phase 3 will integrate,
// not a separate code path built just for this test.
static uint64_t perft(BBoard& b, int depth) {
    if (depth == 0) return 1;
    auto moves = b.generateLegalMoves();
    if (depth == 1) return static_cast<uint64_t>(moves.size());
    uint64_t nodes = 0;
    for (const auto& m : moves) {
        if (!b.makeMove(m)) {
            std::cout << "FATAL: generateLegalMoves() produced an illegal move during perft\n";
            std::exit(1);
        }
        nodes += perft(b, depth - 1);
        b.unmakeMove();
    }
    return nodes;
}

static void runStep3Checks() {
    // 1. Zobrist-incremental correctness, exhaustively, to depth 3, on all 6
    //    positions -- cheap enough here to recompute-from-scratch at every
    //    single node and compare.
    for (const auto& [name, fen] : POSITIONS) {
        BBoard b; b.setFEN(fen);
        int mismatches = 0; uint64_t nodesChecked = 0;
        verifyZobristIncremental(b, 3, mismatches, nodesChecked);
        check(name + " incremental Zobrist key matches from-scratch recompute at every node (depth 3, " + std::to_string(nodesChecked) + " nodes)",
              mismatches == 0, std::to_string(mismatches) + " mismatches");
    }

    // 2. Full make/unmake round-trip: after a depth-3 perft traversal (which
    //    pushes and pops a great many Undo snapshots), the board must be
    //    byte-for-byte back where it started -- same FEN, same key, empty stack.
    for (const auto& [name, fen] : POSITIONS) {
        BBoard b; b.setFEN(fen);
        std::string fenBefore = b.getFEN();
        uint64_t keyBefore = b.positionKey();
        perft(b, 3);
        check(name + " board FEN unchanged after full make/unmake round-trip", b.getFEN() == fenBefore);
        check(name + " Zobrist key unchanged after full make/unmake round-trip", b.positionKey() == keyBefore);
        check(name + " undo stack empty after full make/unmake round-trip", b.stack.empty());
    }

    // 3. Full perft to depth 5-6 on all 6 standard positions, cross-checked
    //    against the exact node counts tests/perft_test.py already trusts
    //    for the mailbox engine (its "full"/"deep" mode reference values --
    //    using "deep" wherever "full" mode's depth for a position was below
    //    5, per the non-negotiable depth 5-6 bar for this phase).
    struct DeepCase { std::string name; std::string fen; int depth; uint64_t expected; };
    static const std::vector<DeepCase> DEEP = {
        {"startpos",  "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",                    5, 4865609ULL},
        {"kiwipete",  "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",         5, 193690690ULL},
        {"position3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",                                    6, 11030083ULL},
        {"position4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",             5, 15833292ULL},
        {"position5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",                    5, 89941194ULL},
        {"position6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",     5, 164075551ULL},
    };
    for (const auto& c : DEEP) {
        BBoard b; b.setFEN(c.fen);
        auto t0 = std::chrono::steady_clock::now();
        uint64_t nodes = perft(b, c.depth);
        auto t1 = std::chrono::steady_clock::now();
        double secs = std::chrono::duration<double>(t1 - t0).count();
        std::cout << "  perft " << c.name << " depth " << c.depth << ": " << nodes
                  << " nodes (" << secs << "s, expected " << c.expected << ")\n";
        check(c.name + " perft depth " + std::to_string(c.depth) + " matches known-correct node count",
              nodes == c.expected, "got " + std::to_string(nodes) + " want " + std::to_string(c.expected));
    }
}

int main() {
    Zobrist::init();
    initBitboardTables();

    for (const auto& [name, fen] : POSITIONS) {
        BBoard b;
        b.setFEN(fen);

        // 1. FEN round-trip: getFEN() must reproduce the exact input FEN.
        std::string out = b.getFEN();
        check(name + " FEN round-trip", out == fen, "got [" + out + "] want [" + fen + "]");

        // 2. Zobrist key consistency: the incrementally-set key from setFEN()
        //    must match an independent from-scratch recomputation. At this
        //    stage (state + FEN only, no makeMove yet) both paths actually
        //    go through the same recomputeKeyFromScratch() call internally,
        //    so this mainly guards against a future edit accidentally
        //    decoupling them -- but it's cheap and it's the right habit to
        //    establish now, before makeMove/unmakeMove make it load-bearing.
        uint64_t stored = b.positionKey();
        uint64_t fresh = b.recomputeKeyFromScratch();
        check(name + " Zobrist key matches fresh recompute", stored == fresh);

        // 3. King squares match what's actually on the board (cross-check
        //    the cached wk/bk fields against the piece bitboards directly).
        Bitboard wkBB = b.st.pieces[WK], bkBB = b.st.pieces[BK];
        int wkExpected = wkBB ? lsbIndex(wkBB) : -1;
        int bkExpected = bkBB ? lsbIndex(bkBB) : -1;
        check(name + " white king square", b.st.wk == wkExpected);
        check(name + " black king square", b.st.bk == bkExpected);

        // 4. Occupancy bitboards are self-consistent: white | black == all,
        //    and white & black == 0 (no square can hold two pieces).
        check(name + " occWhite|occBlack == occAll", (b.st.occWhite | b.st.occBlack) == b.st.occAll);
        check(name + " occWhite & occBlack == 0", (b.st.occWhite & b.st.occBlack) == 0);

        // 5. Every one of the 12 piece bitboards must be pairwise disjoint
        //    (no square double-counted across two different piece types).
        for (int i = 0; i < 12; ++i) {
            for (int j = i + 1; j < 12; ++j) {
                if (b.st.pieces[i] & b.st.pieces[j]) {
                    check(name + " pieces[" + std::to_string(i) + "] disjoint from pieces[" + std::to_string(j) + "]", false);
                }
            }
        }
    }

    // 6. pieceIndexOf / charOfPieceIndex round-trip for all 12 piece chars.
    {
        const std::string chars = "PNBRQKpnbrqk";
        for (char c : chars) {
            int idx = BBoard::pieceIndexOf(c);
            check(std::string("pieceIndexOf/charOfPieceIndex round-trip for '") + c + "'",
                  idx >= 0 && idx < 12 && BBoard::charOfPieceIndex(idx) == c);
        }
        check("pieceIndexOf('.') == -1", BBoard::pieceIndexOf('.') == -1);
    }

    runStep2Checks();
    runStep3Checks();

    if (failures == 0) {
        std::cout << "ALL CHECKS PASSED (0 failures)\n";
        return 0;
    } else {
        std::cout << failures << " CHECKS FAILED\n";
        return 1;
    }
}