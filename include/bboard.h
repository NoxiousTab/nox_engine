#pragma once
#include <array>
#include <string>
#include <cstdint>
#include "types.h"
#include "bitboard.h"

namespace eng {

// ---------------------------------------------------------------------------
// Phase 2: bitboard-based board representation.
//
// Piece indexing DELIBERATELY matches Zobrist::piece[] and the mailbox
// Board's internal pieceIndex() exactly (0=P 1=N 2=B 3=R 4=Q 5=K, then the
// same order for black: 6=p 7=n 8=b 9=r 10=q 11=k). This means a BBoard and
// a mailbox Board holding the identical chess position compute the EXACT
// SAME positionKey() -- which is what lets Phase 2/3 cross-validate BBoard's
// output directly against the mailbox Board's already-trusted perft/search
// results, instead of having two disconnected key spaces to reconcile.
//
// Square indexing matches bitboard.h's convention (index = rank*8+file,
// rank1=row0, a1=0...h8=63), which itself was deliberately chosen in Phase 1
// to match the mailbox Board too. So all three representations (mailbox
// Board, bitboard.h's attack tables, and this BBoard) now agree on both
// square numbering and piece numbering -- no more per-component mapping
// translation, and no repeat of the bug #7 PST-flip class of mistake.
// ---------------------------------------------------------------------------

enum PieceIndex : int {
    WP = 0, WN = 1, WB = 2, WR = 3, WQ = 4, WK = 5,
    BP = 6, BN = 7, BB = 8, BR = 9, BQ = 10, BK = 11
};

struct BBState {
    std::array<Bitboard, 12> pieces{}; // indexed by PieceIndex

    // Derived occupancy bitboards. These are NOT independently mutated bit-
    // by-bit at each move step; they are fully recomputed from `pieces` via
    // BBoard::recomputeOcc() after every mutation (setFEN, and later
    // makeMove/unmakeMove). Recomputing is only 12 OR operations -- trivial
    // cost -- and eliminates an entire class of "forgot to update occupancy
    // on this one code path" bugs in exchange for that negligible overhead.
    Bitboard occWhite{0};
    Bitboard occBlack{0};
    Bitboard occAll{0};

    char side{'w'};      // 'w' or 'b'
    int castling{0};     // bit 1=K, 2=Q, 4=k, 8=q -- same convention as mailbox Board::State
    int ep{-1};           // en passant target square, -1 if none
    int halfmove{0};
    int fullmove{1};
    int wk{-1};           // white king square, cached for O(1) check-detection lookups later
    int bk{-1};           // black king square

    uint64_t zobristKey{0}; // incrementally maintained once makeMove exists (Phase 2 step 3);
                            // for now (step 1: state + FEN only) it's computed once per
                            // setFEN() call via BBoard::recomputeKeyFromScratch().
};

// Full snapshot of BBState taken immediately BEFORE a move is applied. Unlike
// the mailbox Board's Undo (which stores just the handful of fields that
// changed, and manually reverses each mutation in unmakeMove()), this stores
// the entire state wholesale. A BBState is only ~15 machine words, so the
// copy is negligible, and it buys real safety: unmakeMove() can never
// "forget" to reverse one specific mutation a future edit to makeMove()
// adds, because it isn't reversing anything -- it's just restoring the
// snapshot. makeMove() itself still updates zobristKey incrementally (XOR
// in/out per changed piece/castling/ep/side) rather than recomputing from
// scratch, which is the actual performance-relevant property for search.
struct BBUndo {
    BBState prevState;
};

class BBoard {
public:
    BBState st{};
    std::vector<BBUndo> stack;

    void setStartPos();
    void setFEN(const std::string& fen);
    std::string getFEN() const;

    // O(1): returns the incrementally-maintained key. Once makeMove/unmakeMove
    // exist (Phase 2 step 3), this is the one search/perft code should call.
    uint64_t positionKey() const { return st.zobristKey; }

    // O(64): recomputes the Zobrist key completely from scratch by scanning
    // every square, independent of whatever zobristKey currently holds. This
    // is the ground-truth reference used ONLY for cross-validation (asserting
    // positionKey() == recomputeKeyFromScratch() after every setFEN() now,
    // and after every makeMove()/unmakeMove() once those exist) -- never used
    // as the fast path itself.
    uint64_t recomputeKeyFromScratch() const;

    bool isDrawBy50() const { return st.halfmove >= 100; }

    // Returns true if `sq` is attacked by any piece of color `bySide` ('w' or
    // 'b'), given the CURRENT board occupancy. This is a pure query -- it
    // does not know about "who's actually in check right now" as a game
    // concept, it just answers "if a piece belonging to bySide could capture
    // on sq right now, could it". Used by castling generation below (a
    // castle is illegal if the king's start, transit, or destination square
    // is attacked) and, from Phase 2 step 3 onward, by legality filtering
    // (a move is illegal if it leaves the mover's own king attacked).
    bool squareAttacked(int sq, char bySide) const;

    // Pseudo-legal: includes every geometrically-valid move for the side to
    // move (knight/king steps, slider rays via the Phase 1 magic tables,
    // pawn pushes/captures/promotions/en passant, and castling -- castling
    // IS filtered against squareAttacked() here, same as the mailbox Board,
    // since "does this square happen to be attacked right now" is a cheap
    // static query, not a make/unmake-dependent legality check).
    // Deliberately NOT filtered for "does this move leave my own king in
    // check" -- e.g. moving a pinned piece off its pin line, or a piece
    // blocking a check, is still returned here. That legality filter is
    // Phase 2 step 3's job, once make/unmake exists to apply-then-check
    // rather than have every generator hand-roll pin detection.
    std::vector<Move> generatePseudoLegalMoves() const;

    // Applies `m` to the board. Returns false (and fully restores the prior
    // state itself, popping the stack it just pushed) if `m` leaves the
    // mover's own king attacked -- i.e. this is where pseudo-legal moves
    // finally get filtered down to legal ones, by literally playing the move
    // and checking, rather than hand-rolling pin/check detection per piece
    // type the way some engines do. Callers should treat a `false` return as
    // "this move was illegal, board unchanged" and simply skip it.
    bool makeMove(const Move& m);

    // Reverses the most recent makeMove() call by restoring the snapshot it
    // pushed onto `stack`. Undefined behavior if called with an empty stack
    // (asserted in debug builds) -- exactly mirrors the mailbox Board's
    // unmakeMove() contract.
    void unmakeMove();

    // Convenience wrapper: generatePseudoLegalMoves(), filtered down to
    // legal moves by actually calling makeMove()/unmakeMove() on each one.
    // Not const, since makeMove/unmakeMove mutate `st` and `stack` (even
    // though the net effect, once this returns, is that the board is back
    // exactly where it started).
    std::vector<Move> generateLegalMoves();

    // Maps a FEN piece character ('P'..'K','p'..'k') to its PieceIndex.
    // Returns -1 for any other character (e.g. '.', a bug in caller logic).
    static int pieceIndexOf(char c);
    // Inverse of pieceIndexOf: maps a PieceIndex (0..11) back to its FEN char.
    static char charOfPieceIndex(int idx);

private:
    void recomputeOcc();

    void genPawnMoves(std::vector<Move>& out) const;
    void genKnightMoves(std::vector<Move>& out) const;
    void genBishopMoves(std::vector<Move>& out) const;
    void genRookMoves(std::vector<Move>& out) const;
    void genQueenMoves(std::vector<Move>& out) const;
    void genKingMoves(std::vector<Move>& out) const;   // also calls genCastleMoves
    void genCastleMoves(std::vector<Move>& out) const;
};

}