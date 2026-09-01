#include <iostream>
#include <random>
#include <chrono>
#include "bitboard.h"

using namespace eng;

static int failures = 0;

static void checkEq(const std::string& label, Bitboard a, Bitboard b) {
    if (a != b) {
        std::cout << "MISMATCH [" << label << "]: got=0x" << std::hex << a
                  << " expected=0x" << b << std::dec << "\n";
        ++failures;
    }
}

// A completely independent, hand-verified reference for knight attacks from
// a handful of well-known squares, computed by listing target squares
// manually rather than via any shared code path.
static Bitboard bbFromSquares(std::initializer_list<int> sqs) {
    Bitboard bb = 0;
    for (int s : sqs) bb |= sqBit(s);
    return bb;
}

int main() {
    initBitboardTables();

    // --- Knight attacks: hand-verified spot checks ---
    // Knight on a1 (sq 0): only b3 (17) and c2 (10) are reachable.
    checkEq("knight a1", KNIGHT_ATTACKS[0], bbFromSquares({17, 10}));
    // Knight on h1 (sq 7): only f2 (13) and g3 (22).
    checkEq("knight h1", KNIGHT_ATTACKS[7], bbFromSquares({13, 22}));
    // Knight on d4 (sq 27, file d=3 rank4=3 -> 3*8+3=27): 8 targets.
    // b3(17) b5(33) c2(10) c6(42) e2(12) e6(44) f3(21) f5(37)
    checkEq("knight d4", KNIGHT_ATTACKS[27], bbFromSquares({17, 33, 10, 42, 12, 44, 21, 37}));

    // --- King attacks: spot checks ---
    checkEq("king a1", KING_ATTACKS[0], bbFromSquares({1, 8, 9}));
    checkEq("king h8", KING_ATTACKS[63], bbFromSquares({62, 54, 55}));
    checkEq("king e4 (28)", KING_ATTACKS[28], bbFromSquares({27, 29, 19, 20, 21, 35, 36, 37}));

    // --- Pawn attacks: spot checks ---
    // White pawn on e4 (28): attacks d5(35) and f5(37).
    checkEq("white pawn e4", PAWN_ATTACKS[0][28], bbFromSquares({35, 37}));
    // Black pawn on e5 (36): attacks d4(27) and f4(29).
    checkEq("black pawn e5", PAWN_ATTACKS[1][36], bbFromSquares({27, 29}));
    // Edge case: white pawn on a2 (8) only attacks b3(17), no wraparound to h-file.
    checkEq("white pawn a2 (no wrap)", PAWN_ATTACKS[0][8], bbFromSquares({17}));
    // Edge case: white pawn on h2 (15) only attacks g3(22), no wraparound to a-file.
    checkEq("white pawn h2 (no wrap)", PAWN_ATTACKS[0][15], bbFromSquares({22}));

    // --- Magic bitboard sliding attacks: exhaustive random validation ---
    // For every square, generate many random occupancy bitboards and check
    // that the magic-based lookup exactly matches the slow ray-tracer.
    std::mt19937_64 rng(12345);
    int totalChecks = 0;
    for (int sq = 0; sq < 64; ++sq) {
        for (int trial = 0; trial < 5000; ++trial) {
            Bitboard occ = rng() & rng(); // sparser random occupancy, more realistic
            Bitboard gotR = rookAttacks(sq, occ);
            Bitboard wantR = rookAttacksSlow(sq, occ);
            if (gotR != wantR) {
                std::cout << "ROOK MISMATCH sq=" << sq << " occ=0x" << std::hex << occ
                          << " got=0x" << gotR << " want=0x" << wantR << std::dec << "\n";
                ++failures;
            }
            Bitboard gotB = bishopAttacks(sq, occ);
            Bitboard wantB = bishopAttacksSlow(sq, occ);
            if (gotB != wantB) {
                std::cout << "BISHOP MISMATCH sq=" << sq << " occ=0x" << std::hex << occ
                          << " got=0x" << gotB << " want=0x" << wantB << std::dec << "\n";
                ++failures;
            }
            totalChecks += 2;
        }
    }
    std::cout << "Ran " << totalChecks << " random sliding-attack checks across all 64 squares.\n";

    // --- Specific known sliding positions, hand-verified ---
    // Rook on a1 (0) with a blocker on a4 (24, i.e. bit 24) and d1 (3):
    // should see a2,a3,a4 (stops at blocker) and b1,c1,d1 (stops at blocker).
    {
        Bitboard occ = sqBit(24) | sqBit(3);
        Bitboard want = bbFromSquares({8, 16, 24, 1, 2, 3});
        checkEq("rook a1 with blockers", rookAttacks(0, occ), want);
    }
    // Bishop on d4 (27) with a blocker on f6 (45) along the a1-h8-ish diagonal
    // (d4->e5->f6->g7->h8), should stop at f6 inclusive, and see the full
    // unobstructed extent of its other three diagonal rays.
    {
        Bitboard occ = sqBit(45);
        Bitboard want = bbFromSquares({
            36, 45,               // d4->e5->f6 (blocked, inclusive)
            18, 9, 0,             // d4->c3->b2->a1
            20, 13, 6,            // d4->e3->f2->g1
            34, 41, 48            // d4->c5->b6->a7
        });
        checkEq("bishop d4 with blocker", bishopAttacks(27, occ), want);
    }

    // --- Timing sanity check ---
    auto t0 = std::chrono::steady_clock::now();
    volatile Bitboard sink = 0;
    for (int i = 0; i < 10000000; ++i) {
        sink ^= rookAttacks(i & 63, rng());
    }
    auto t1 = std::chrono::steady_clock::now();
    double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    std::cout << "10M rookAttacks() magic lookups took " << us / 1000.0 << " ms ("
              << (10000000.0 / (us / 1e6) / 1e6) << " M lookups/sec)\n";

    if (failures == 0) {
        std::cout << "\nALL CHECKS PASSED (0 failures)\n";
        return 0;
    } else {
        std::cout << "\n" << failures << " CHECKS FAILED\n";
        return 1;
    }
}

