#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <tuple>
#include "bboard.h"
#include "board.h"
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

// ---------------------------------------------------------------------------
// Phase 3 step 1: cross-check the newly-added BBoard API (pieceCharAt,
// flatChars, generateCaptures, makeNullMove/unmakeNullMove, repetitionCount,
// see) directly against the mailbox Board's already-trusted behavior.
// ---------------------------------------------------------------------------

using MoveTuple = std::tuple<int,int,char,uint16_t>;

static std::vector<MoveTuple> toTuples(const std::vector<Move>& moves) {
    std::vector<MoveTuple> t;
    t.reserve(moves.size());
    for (const auto& m : moves) t.emplace_back(m.from, m.to, m.promo, m.flags);
    std::sort(t.begin(), t.end());
    return t;
}

static void runPhase3Step1Checks() {
    // 1. pieceCharAt()/flatChars() must agree with the mailbox Board's own
    //    st.board[] array, square for square, on all 6 standard positions.
    for (const auto& [name, fen] : POSITIONS) {
        BBoard bb; bb.setFEN(fen);
        Board mb; mb.setFEN(fen);
        auto flat = bb.flatChars();
        for (int sq = 0; sq < 64; ++sq) {
            check(name + " pieceCharAt matches mailbox Board sq=" + std::to_string(sq),
                  bb.pieceCharAt(sq) == mb.st.board[sq]);
            check(name + " flatChars matches mailbox Board sq=" + std::to_string(sq),
                  flat[sq] == mb.st.board[sq]);
        }
    }

    // 2. generateCaptures() must return the exact same set of moves (as
    //    from/to/promo/flags tuples, order-independent) as the mailbox
    //    Board's generateCaptures() on all 6 standard positions.
    for (const auto& [name, fen] : POSITIONS) {
        BBoard bb; bb.setFEN(fen);
        Board mb; mb.setFEN(fen);
        auto bbCaps = toTuples(bb.generateCaptures());
        auto mbCaps = toTuples(mb.generateCaptures());
        check(name + " generateCaptures() matches mailbox Board (same move set)",
              bbCaps == mbCaps,
              "BBoard had " + std::to_string(bbCaps.size()) + " moves, mailbox had " + std::to_string(mbCaps.size()));
    }

    // 3. makeNullMove()/unmakeNullMove(): same accept/reject decision as the
    //    mailbox Board (reject only when the side to move is in check), and
    //    when accepted, the resulting position (and its Zobrist key -- valid
    //    to compare directly, since both representations share the exact
    //    same Zobrist tables) matches exactly. Then confirm unmake restores
    //    both boards to their original FEN/key.
    for (const auto& [name, fen] : POSITIONS) {
        BBoard bb; bb.setFEN(fen);
        Board mb; mb.setFEN(fen);
        std::string fenBefore = bb.getFEN();
        uint64_t keyBefore = bb.positionKey();

        bool bbOk = bb.makeNullMove();
        bool mbOk = mb.makeNullMove();
        check(name + " makeNullMove() accept/reject matches mailbox Board", bbOk == mbOk);

        if (bbOk && mbOk) {
            check(name + " makeNullMove() resulting FEN matches mailbox Board", bb.getFEN() == mb.getFEN());
            check(name + " makeNullMove() resulting Zobrist key matches mailbox Board", bb.positionKey() == mb.positionKey());
            bb.unmakeNullMove();
            mb.unmakeNullMove();
            check(name + " unmakeNullMove() restores original FEN", bb.getFEN() == fenBefore);
            check(name + " unmakeNullMove() restores original Zobrist key", bb.positionKey() == keyBefore);
        }
    }

    // 4. repetitionCount(): play a knight shuffle from startpos that returns
    //    to the starting position twice (Nc3-b1, Nb1-c3 mirrored by black),
    //    and confirm BBoard's count matches the mailbox Board's count after
    //    every single ply of the sequence, not just at the end.
    {
        BBoard bb; bb.setStartPos();
        Board mb; mb.setStartPos();
        // g1f3, g8f6, f3g1, f6g8, g1f3, g8f6, f3g1, f6g8  -- position after
        // every 4th ply (back to two knights on their home squares, same
        // side to move) repeats the starting position.
        static const std::vector<std::pair<std::string,std::string>> plies = {
            {"g1", "f3"}, {"g8", "f6"}, {"f3", "g1"}, {"f6", "g8"},
            {"g1", "f3"}, {"g8", "f6"}, {"f3", "g1"}, {"f6", "g8"},
        };
        for (size_t i = 0; i < plies.size(); ++i) {
            int from = coordToSq(plies[i].first);
            int to = coordToSq(plies[i].second);

            auto bbMoves = bb.generateLegalMoves();
            Move bbMove{};
            for (const auto& m : bbMoves) if (m.from == from && m.to == to) { bbMove = m; break; }
            bb.makeMove(bbMove);

            auto mbMoves = mb.generateLegalMoves();
            Move mbMove{};
            for (const auto& m : mbMoves) if (m.from == from && m.to == to) { mbMove = m; break; }
            mb.makeMove(mbMove);

            check("repetitionCount matches mailbox Board after ply " + std::to_string(i + 1),
                  bb.repetitionCount() == mb.repetitionCount(),
                  "BBoard=" + std::to_string(bb.repetitionCount()) + " mailbox=" + std::to_string(mb.repetitionCount()));
        }
        check("repetitionCount reaches 3 after the full repeated shuffle", bb.repetitionCount() == 3);
    }

    // 5. see(): the full 73-case suite from tests/see_test.py (the same
    //    suite tests/see_test.py runs against the mailbox engine's UCI `see`
    //    command), run directly through BBoard::see(). Since BBoard::see()
    //    bridges to Board::see() via a FEN round-trip, this mainly validates
    //    that generateLegalMoves() finds and flags the right move (from a
    //    bare UCI move string) on these 73 diverse tactical positions --
    //    distinct positions from the 6 standard ones used everywhere else in
    //    this harness -- and that the round-trip bridge itself works.
    struct SeeCase { std::string fen; std::string uci; int expected; };
    static const std::vector<SeeCase> SEE_CASES = {
        {"6k1/1pp4p/p1pb4/6q1/3P1pRr/2P4P/PP1Br1P1/5RKN w - -", "f1f4", -100},
        {"5rk1/1pp2q1p/p1pb4/8/3P1NP1/2P5/1P1BQ1P1/5RK1 b - -", "d6f4", 0},
        {"4R3/2r3p1/5bk1/1p1r3p/p2PR1P1/P1BK1P2/1P6/8 b - -", "h5g4", 0},
        {"4R3/2r3p1/5bk1/1p1r1p1p/p2PR1P1/P1BK1P2/1P6/8 b - -", "h5g4", 0},
        {"4r1k1/5pp1/nbp4p/1p2p2q/1P2P1b1/1BP2N1P/1B2QPPK/3R4 b - -", "g4f3", 0},
        {"2r1r1k1/pp1bppbp/3p1np1/q3P3/2P2P2/1P2B3/P1N1B1PP/2RQ1RK1 b - -", "d6e5", 100},
        {"7r/5qpk/p1Qp1b1p/3r3n/BB3p2/5p2/P1P2P2/4RK1R w - -", "e1e8", 0},
        {"6rr/6pk/p1Qp1b1p/2n5/1B3p2/5p2/P1P2P2/4RK1R w - -", "e1e8", -500},
        {"7r/5qpk/2Qp1b1p/1N1r3n/BB3p2/5p2/P1P2P2/4RK1R w - -", "e1e8", -500},
        {"6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - -", "f7f8q", 200},
        {"6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - -", "f7f8n", 200},
        {"7R/5P2/8/8/6r1/3K4/5p2/4k3 w - -", "f7f8q", 800},
        {"7R/5P2/8/8/6r1/3K4/5p2/4k3 w - -", "f7f8b", 200},
        {"7R/4bP2/8/8/1q6/3K4/5p2/4k3 w - -", "f7f8r", -100},
        {"8/4kp2/2npp3/1Nn5/1p2PQP1/7q/1PP1B3/4KR1r b - -", "h1f1", 0},
        {"8/4kp2/2npp3/1Nn5/1p2P1P1/7q/1PP1B3/4KR1r b - -", "h1f1", 0},
        {"2r2r1k/6bp/p7/2q2p1Q/3PpP2/1B6/P5PP/2RR3K b - -", "c5c1", 100},
        {"r2qk1nr/pp2ppbp/2b3p1/2p1p3/8/2N2N2/PPPP1PPP/R1BQR1K1 w kq -", "f3e5", 100},
        {"6r1/4kq2/b2p1p2/p1pPb3/p1P2B1Q/2P4P/2B1R1P1/6K1 w - -", "f4e5", 0},
        {"3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R4B/PQ3P1P/3R2K1 w - h6", "g5h6", 0},
        {"3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R1B2B/PQ3P1P/3R2K1 w - h6", "g5h6", 100},
        {"2r4r/1P4pk/p2p1b1p/7n/BB3p2/2R2p2/P1P2P2/4RK2 w - -", "c3c8", 500},
        {"2r5/1P4pk/p2p1b1p/5b1n/BB3p2/2R2p2/P1P2P2/4RK2 w - -", "c3c8", 500},
        {"2r4k/2r4p/p7/2b2p1b/4pP2/1BR5/P1R3PP/2Q4K w - -", "c3c5", 300},
        {"8/pp6/2pkp3/4bp2/2R3b1/2P5/PP4B1/1K6 w - -", "g2c6", -200},
        {"4q3/1p1pr1k1/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - -", "e6e4", -400},
        {"4q3/1p1pr1kb/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - -", "h7e4", 100},
        {"3r3k/3r4/2n1n3/8/3p4/2PR4/1B1Q4/3R3K w - -", "d3d4", -100},
        {"1k1r4/1ppn3p/p4b2/4n3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -", "d3e5", 100},
        {"1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -", "d3e5", -200},
        {"rnb2b1r/ppp2kpp/5n2/4P3/q2P3B/5R2/PPP2PPP/RN1QKB2 w Q -", "h4f6", 100},
        {"r2q1rk1/2p1bppp/p2p1n2/1p2P3/4P1b1/1nP1BN2/PP3PPP/RN1QR1K1 b - -", "g4f3", 0},
        {"r1bqkb1r/2pp1ppp/p1n5/1p2p3/3Pn3/1B3N2/PPP2PPP/RNBQ1RK1 b kq -", "c6d4", 0},
        {"r1bq1r2/pp1ppkbp/4N1p1/n3P1B1/8/2N5/PPP2PPP/R2QK2R w KQ -", "e6g7", 0},
        {"r1bq1r2/pp1ppkbp/4N1pB/n3P3/8/2N5/PPP2PPP/R2QK2R w KQ -", "e6g7", 300},
        {"rnq1k2r/1b3ppp/p2bpn2/1p1p4/3N4/1BN1P3/PPP2PPP/R1BQR1K1 b kq -", "d6h2", -200},
        {"rn2k2r/1bq2ppp/p2bpn2/1p1p4/3N4/1BN1P3/PPP2PPP/R1BQR1K1 b kq -", "d6h2", 100},
        {"r2qkbn1/ppp1pp1p/3p1rp1/3Pn3/4P1b1/2N2N2/PPP2PPP/R1BQKB1R b KQq -", "g4f3", 100},
        {"rnbq1rk1/pppp1ppp/4pn2/8/1bPP4/P1N5/1PQ1PPPP/R1B1KBNR b KQ -", "b4c3", 0},
        {"r4rk1/3nppbp/bq1p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - -", "b6b2", -800},
        {"r4rk1/1q1nppbp/b2p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - -", "f6d5", -200},
        {"1r3r2/5p2/4p2p/2k1n1P1/2PN1nP1/1P3P2/8/2KR1B1R b - -", "b8b3", -400},
        {"1r3r2/5p2/4p2p/4n1P1/kPPN1nP1/5P2/8/2KR1B1R b - -", "b8b4", 100},
        {"2r2rk1/5pp1/pp5p/q2p4/P3n3/1Q3NP1/1P2PP1P/2RR2K1 b - -", "c8c1", 0},
        {"5rk1/5pp1/2r4p/5b2/2R5/6Q1/R1P1qPP1/5NK1 b - -", "f5c2", -100},
        {"1r3r1k/p4pp1/2p1p2p/qpQP3P/2P5/3R4/PP3PP1/1K1R4 b - -", "a5a2", -800},
        {"1r5k/p4pp1/2p1p2p/qpQP3P/2P2P2/1P1R4/P4rP1/1K1R4 b - -", "a5a2", 100},
        {"r2q1rk1/1b2bppp/p2p1n2/1ppNp3/3nP3/P2P1N1P/BPP2PP1/R1BQR1K1 w - -", "d5e7", 0},
        {"rnbqrbn1/pp3ppp/3p4/2p2k2/4p3/3B1K2/PPP2PPP/RNB1Q1NR w - -", "d3e4", 100},
        {"rnb1k2r/p3p1pp/1p3p1b/7n/1N2N3/3P1PB1/PPP1P1PP/R2QKB1R w KQkq -", "e4d6", -200},
        {"r1b1k2r/p4npp/1pp2p1b/7n/1N2N3/3P1PB1/PPP1P1PP/R2QKB1R w KQkq -", "e4d6", 0},
        {"2r1k2r/pb4pp/5p1b/2KB3n/4N3/2NP1PB1/PPP1P1PP/R2Q3R w k -", "d5c6", -300},
        {"2r1k2r/pb4pp/5p1b/2KB3n/1N2N3/3P1PB1/PPP1P1PP/R2Q3R w k -", "d5c6", 0},
        {"2r1k3/pbr3pp/5p1b/2KB3n/1N2N3/3P1PB1/PPP1P1PP/R2Q3R w - -", "d5c6", -300},
        {"5k2/p2P2pp/8/1pb5/1Nn1P1n1/6Q1/PPP4P/R3K1NR w KQ -", "d7d8q", 800},
        {"r4k2/p2P2pp/8/1pb5/1Nn1P1n1/6Q1/PPP4P/R3K1NR w KQ -", "d7d8q", -100},
        {"5k2/p2P2pp/1b6/1p6/1Nn1P1n1/8/PPP4P/R2QK1NR w KQ -", "d7d8q", 200},
        {"4kbnr/p1P1pppp/b7/4q3/7n/8/PP1PPPPP/RNBQKBNR w KQk -", "c7c8q", -100},
        {"4kbnr/p1P1pppp/b7/4q3/7n/8/PPQPPPPP/RNB1KBNR w KQk -", "c7c8q", 200},
        {"4kbnr/p1P1pppp/b7/4q3/7n/8/PPQPPPPP/RNB1KBNR w KQk -", "c7c8q", 200},
        {"4kbnr/p1P4p/b1q5/5pP1/4n3/5Q2/PP1PPP1P/RNB1KBNR w KQk f6", "g5f6", 0},
        {"4kbnr/p1P4p/b1q5/5pP1/4n3/5Q2/PP1PPP1P/RNB1KBNR w KQk f6", "g5f6", 0},
        {"4kbnr/p1P4p/b1q5/5pP1/4n2Q/8/PP1PPP1P/RNB1KBNR w KQk f6", "g5f6", 0},
        {"1n2kb1r/p1P4p/2qb4/5pP1/4n2Q/8/PP1PPP1P/RNB1KBNR w KQk -", "c7b8q", 200},
        {"rnbqk2r/pp3ppp/2p1pn2/3p4/3P4/N1P1BN2/PPB1PPPb/R2Q1RK1 w kq -", "g1h2", 300},
        {"3N4/2K5/2n5/1k6/8/8/8/8 b - -", "c6d8", 0},
        {"3N4/2P5/2n5/1k6/8/8/8/4K3 b - -", "c6d8", -800},
        {"3n3r/2P5/8/1k6/8/8/3Q4/4K3 w - -", "d2d8", 300},
        {"3n3r/2P5/8/1k6/8/8/3Q4/4K3 w - -", "c7d8q", 700},
        {"r2n3r/2P1P3/4N3/1k6/8/8/8/4K3 w - -", "e6d8", 300},
        {"8/8/8/1k6/6b1/4N3/2p3K1/3n4 w - -", "e3d1", 0},
        {"8/8/1k6/8/8/2N1N3/4p1K1/3n4 w - -", "c3d1", 100},
        {"r1bqk1nr/pppp1ppp/2n5/1B2p3/1b2P3/5N2/PPPP1PPP/RNBQK2R w KQkq -", "e1g1", 0},
    };
    int seeChecked = 0;
    for (const auto& c : SEE_CASES) {
        BBoard bb; bb.setFEN(c.fen);
        int from = coordToSq(c.uci.substr(0, 2));
        int to = coordToSq(c.uci.substr(2, 2));
        char promo = 0;
        if (c.uci.size() >= 5) {
            char pc = std::tolower(c.uci[4]);
            if (pc == 'q' || pc == 'r' || pc == 'b' || pc == 'n')
                promo = (bb.st.side == 'w') ? std::toupper(pc) : pc;
        }
        auto moves = bb.generateLegalMoves();
        Move found{}; bool foundAny = false;
        for (const auto& m : moves) {
            if (m.from != from || m.to != to) continue;
            if (m.flags & PROMOTION) { if (promo && m.promo == promo) { found = m; foundAny = true; break; } else continue; }
            found = m; foundAny = true; break;
        }
        check("SEE case " + c.uci + " (" + c.fen + "): move found among legal moves", foundAny);
        if (foundAny) {
            int got = bb.see(found);
            check("SEE case " + c.uci + " (" + c.fen + ")",
                  got == c.expected, "got " + std::to_string(got) + " want " + std::to_string(c.expected));
        }
        ++seeChecked;
    }
    std::cout << "  see(): checked " << seeChecked << " cases from tests/see_test.py's suite\n";
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
    runPhase3Step1Checks();

    if (failures == 0) {
        std::cout << "ALL CHECKS PASSED (0 failures)\n";
        return 0;
    } else {
        std::cout << failures << " CHECKS FAILED\n";
        return 1;
    }
}