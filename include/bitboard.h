#pragma once
#include <cstdint>
#include <array>
#include <vector>

namespace eng {

// ---------------------------------------------------------------------------
// Square indexing: a1=0, b1=1, ..., h1=7, a2=8, ..., h8=63.
// This deliberately matches the EXISTING mailbox Board's convention exactly
// (index = rank*8 + file, rank1=row0) so that PST tables, eval logic, and
// anything else that already assumes this indexing carries over without
// another round of the square-mapping confusion found earlier this session.
// ---------------------------------------------------------------------------

using Bitboard = uint64_t;

constexpr Bitboard FILE_A = 0x0101010101010101ULL;
constexpr Bitboard FILE_B = FILE_A << 1;
constexpr Bitboard FILE_G = FILE_A << 6;
constexpr Bitboard FILE_H = FILE_A << 7;
constexpr Bitboard RANK_1 = 0x00000000000000FFULL;
constexpr Bitboard RANK_2 = RANK_1 << 8;
constexpr Bitboard RANK_4 = RANK_1 << 24;
constexpr Bitboard RANK_5 = RANK_1 << 32;
constexpr Bitboard RANK_7 = RANK_1 << 48;
constexpr Bitboard RANK_8 = RANK_1 << 56;

inline int fileOfSq(int sq) { return sq & 7; }
inline int rankOfSq(int sq) { return sq >> 3; }
inline int makeSquare(int file, int rank) { return rank * 8 + file; }
inline bool onBoard(int file, int rank) { return file >= 0 && file < 8 && rank >= 0 && rank < 8; }

inline int popcount(Bitboard b) { return __builtin_popcountll(b); }
inline int lsbIndex(Bitboard b) { return __builtin_ctzll(b); } // undefined if b==0
inline int popLsb(Bitboard& b) { int s = lsbIndex(b); b &= (b - 1); return s; }
inline Bitboard sqBit(int sq) { return 1ULL << sq; }

// Precomputed, non-sliding attack tables.
extern std::array<Bitboard, 64> KNIGHT_ATTACKS;
extern std::array<Bitboard, 64> KING_ATTACKS;
extern std::array<std::array<Bitboard, 64>, 2> PAWN_ATTACKS; // [color][sq]; color 0=white,1=black

// Ground-truth (slow, simple ray-tracing) sliding attack generators. These
// are the reference implementation used to both build and validate the
// magic bitboard tables below -- deliberately written with explicit
// file/rank arithmetic and bounds checks (not raw index offsets) to avoid
// the exact class of wraparound bug found and fixed in the mailbox engine
// earlier this session.
Bitboard rookAttacksSlow(int sq, Bitboard occupied);
Bitboard bishopAttacksSlow(int sq, Bitboard occupied);

// Magic bitboard entry for one square.
struct Magic {
    Bitboard mask{0};     // relevant occupancy mask (excludes outer edge squares)
    Bitboard magic{0};    // magic multiplier
    int shift{0};         // shift amount = 64 - popcount(mask)
    int offset{0};        // offset into the shared attack table for this square
};

extern std::array<Magic, 64> ROOK_MAGICS;
extern std::array<Magic, 64> BISHOP_MAGICS;
extern std::vector<Bitboard> ROOK_ATTACK_TABLE;
extern std::vector<Bitboard> BISHOP_ATTACK_TABLE;

inline Bitboard rookAttacks(int sq, Bitboard occupied) {
    const Magic& m = ROOK_MAGICS[sq];
    uint64_t idx = ((occupied & m.mask) * m.magic) >> m.shift;
    return ROOK_ATTACK_TABLE[m.offset + idx];
}
inline Bitboard bishopAttacks(int sq, Bitboard occupied) {
    const Magic& m = BISHOP_MAGICS[sq];
    uint64_t idx = ((occupied & m.mask) * m.magic) >> m.shift;
    return BISHOP_ATTACK_TABLE[m.offset + idx];
}
inline Bitboard queenAttacks(int sq, Bitboard occupied) {
    return rookAttacks(sq, occupied) | bishopAttacks(sq, occupied);
}

// Must be called once at startup before any of the above attack functions
// (other than the *Slow ground-truth ones) are used. Builds the knight/king/
// pawn tables and searches for + builds the rook/bishop magic tables.
void initBitboardTables();

}