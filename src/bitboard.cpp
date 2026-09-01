#include "bitboard.h"
#include <random>
#include <vector>
#include <algorithm>

namespace eng {

std::array<Bitboard, 64> KNIGHT_ATTACKS{};
std::array<Bitboard, 64> KING_ATTACKS{};
std::array<std::array<Bitboard, 64>, 2> PAWN_ATTACKS{};

std::array<Magic, 64> ROOK_MAGICS{};
std::array<Magic, 64> BISHOP_MAGICS{};
std::vector<Bitboard> ROOK_ATTACK_TABLE;
std::vector<Bitboard> BISHOP_ATTACK_TABLE;

static void initKnightAttacks() {
    static const int df[8] = {1, 2, 2, 1, -1, -2, -2, -1};
    static const int dr[8] = {2, 1, -1, -2, -2, -1, 1, 2};
    for (int sq = 0; sq < 64; ++sq) {
        int f = fileOfSq(sq), r = rankOfSq(sq);
        Bitboard bb = 0;
        for (int i = 0; i < 8; ++i) {
            int nf = f + df[i], nr = r + dr[i];
            if (onBoard(nf, nr)) bb |= sqBit(makeSquare(nf, nr));
        }
        KNIGHT_ATTACKS[sq] = bb;
    }
}

static void initKingAttacks() {
    for (int sq = 0; sq < 64; ++sq) {
        int f = fileOfSq(sq), r = rankOfSq(sq);
        Bitboard bb = 0;
        for (int ddf = -1; ddf <= 1; ++ddf) {
            for (int ddr = -1; ddr <= 1; ++ddr) {
                if (ddf == 0 && ddr == 0) continue;
                int nf = f + ddf, nr = r + ddr;
                if (onBoard(nf, nr)) bb |= sqBit(makeSquare(nf, nr));
            }
        }
        KING_ATTACKS[sq] = bb;
    }
}

static void initPawnAttacks() {
    for (int sq = 0; sq < 64; ++sq) {
        int f = fileOfSq(sq), r = rankOfSq(sq);
        // White pawns attack diagonally forward (toward higher ranks).
        Bitboard w = 0;
        if (onBoard(f - 1, r + 1)) w |= sqBit(makeSquare(f - 1, r + 1));
        if (onBoard(f + 1, r + 1)) w |= sqBit(makeSquare(f + 1, r + 1));
        PAWN_ATTACKS[0][sq] = w;
        // Black pawns attack diagonally forward toward lower ranks.
        Bitboard b = 0;
        if (onBoard(f - 1, r - 1)) b |= sqBit(makeSquare(f - 1, r - 1));
        if (onBoard(f + 1, r - 1)) b |= sqBit(makeSquare(f + 1, r - 1));
        PAWN_ATTACKS[1][sq] = b;
    }
}

Bitboard rookAttacksSlow(int sq, Bitboard occupied) {
    int f = fileOfSq(sq), r = rankOfSq(sq);
    Bitboard bb = 0;
    static const int ddf[4] = {1, -1, 0, 0};
    static const int ddr[4] = {0, 0, 1, -1};
    for (int d = 0; d < 4; ++d) {
        int nf = f + ddf[d], nr = r + ddr[d];
        while (onBoard(nf, nr)) {
            int s = makeSquare(nf, nr);
            bb |= sqBit(s);
            if (occupied & sqBit(s)) break; // blocked -- ray stops here (inclusive of the blocker)
            nf += ddf[d]; nr += ddr[d];
        }
    }
    return bb;
}

Bitboard bishopAttacksSlow(int sq, Bitboard occupied) {
    int f = fileOfSq(sq), r = rankOfSq(sq);
    Bitboard bb = 0;
    static const int ddf[4] = {1, 1, -1, -1};
    static const int ddr[4] = {1, -1, 1, -1};
    for (int d = 0; d < 4; ++d) {
        int nf = f + ddf[d], nr = r + ddr[d];
        while (onBoard(nf, nr)) {
            int s = makeSquare(nf, nr);
            bb |= sqBit(s);
            if (occupied & sqBit(s)) break;
            nf += ddf[d]; nr += ddr[d];
        }
    }
    return bb;
}

// Relevant-occupancy mask: same as the slow ray, but excluding the actual
// edge square in each direction, since a blocker there can't hide anything
// further (there's nothing further, it's the edge) so its presence/absence
// never changes the attack set. Excluding it keeps the mask (and therefore
// the table size) as small as possible.
static Bitboard rookMask(int sq) {
    int f = fileOfSq(sq), r = rankOfSq(sq);
    Bitboard bb = 0;
    for (int nf = f + 1; nf <= 6; ++nf) bb |= sqBit(makeSquare(nf, r));
    for (int nf = f - 1; nf >= 1; --nf) bb |= sqBit(makeSquare(nf, r));
    for (int nr = r + 1; nr <= 6; ++nr) bb |= sqBit(makeSquare(f, nr));
    for (int nr = r - 1; nr >= 1; --nr) bb |= sqBit(makeSquare(f, nr));
    return bb;
}
static Bitboard bishopMask(int sq) {
    int f = fileOfSq(sq), r = rankOfSq(sq);
    Bitboard bb = 0;
    for (int nf = f + 1, nr = r + 1; nf <= 6 && nr <= 6; ++nf, ++nr) bb |= sqBit(makeSquare(nf, nr));
    for (int nf = f + 1, nr = r - 1; nf <= 6 && nr >= 1; ++nf, --nr) bb |= sqBit(makeSquare(nf, nr));
    for (int nf = f - 1, nr = r + 1; nf >= 1 && nr <= 6; --nf, ++nr) bb |= sqBit(makeSquare(nf, nr));
    for (int nf = f - 1, nr = r - 1; nf >= 1 && nr >= 1; --nf, --nr) bb |= sqBit(makeSquare(nf, nr));
    return bb;
}

// Enumerates every subset of `mask` via the standard "carry-rippler" trick.
static std::vector<Bitboard> enumerateSubsets(Bitboard mask) {
    std::vector<Bitboard> subsets;
    Bitboard subset = 0;
    do {
        subsets.push_back(subset);
        subset = (subset - mask) & mask;
    } while (subset != 0);
    return subsets;
}

// Finds a valid magic number for one square by random search: try candidates
// until one maps every occupancy subset of `mask` to a table slot without
// ever mapping two DIFFERENT attack sets to the same slot. This is the
// standard, well-documented magic bitboard construction technique,
// reliably finds a working magic in well under a second per square.
template <typename SlowFn>
static Magic findMagic(int sq, Bitboard mask, SlowFn slowAttacks, std::vector<Bitboard>& table, int& offsetCursor) {
    int bits = popcount(mask);
    int shift = 64 - bits;
    auto subsets = enumerateSubsets(mask);
    std::vector<Bitboard> attacksForSubset(subsets.size());
    for (size_t i = 0; i < subsets.size(); ++i) attacksForSubset[i] = slowAttacks(sq, subsets[i]);

    std::mt19937_64 rng(0x9E3779B97F4A7C15ULL + sq * 0x2545F4914F6CDD1DULL);
    std::vector<Bitboard> used(subsets.size());
    for (;;) {
        Bitboard candidate = rng() & rng() & rng(); // sparse random candidate, standard heuristic
        if (popcount((candidate * mask) >> 56) < 6) continue; // quick reject: needs enough high-bit spread
        std::fill(used.begin(), used.end(), ~0ULL); // sentinel meaning "unfilled" (0 is a legitimate valid attack value for an unoccupied-mask edge case, so use all-ones as the sentinel since a rook/bishop attack bitboard can never legitimately equal ~0ULL)
        bool ok = true;
        for (size_t i = 0; i < subsets.size() && ok; ++i) {
            uint64_t idx = (subsets[i] * candidate) >> shift;
            if (used[idx] == ~0ULL) {
                used[idx] = attacksForSubset[i];
            } else if (used[idx] != attacksForSubset[i]) {
                ok = false;
            }
        }
        if (!ok) continue;

        Magic m;
        m.mask = mask;
        m.magic = candidate;
        m.shift = shift;
        m.offset = offsetCursor;
        table.resize(offsetCursor + subsets.size());
        for (size_t i = 0; i < subsets.size(); ++i) {
            uint64_t idx = (subsets[i] * candidate) >> shift;
            table[offsetCursor + idx] = attacksForSubset[i];
        }
        offsetCursor += (int)subsets.size();
        return m;
    }
}

static bool tablesInitialized = false;

void initBitboardTables() {
    if (tablesInitialized) return;
    initKnightAttacks();
    initKingAttacks();
    initPawnAttacks();

    ROOK_ATTACK_TABLE.clear();
    BISHOP_ATTACK_TABLE.clear();
    int rookCursor = 0, bishopCursor = 0;
    for (int sq = 0; sq < 64; ++sq) {
        ROOK_MAGICS[sq] = findMagic(sq, rookMask(sq), rookAttacksSlow, ROOK_ATTACK_TABLE, rookCursor);
        BISHOP_MAGICS[sq] = findMagic(sq, bishopMask(sq), bishopAttacksSlow, BISHOP_ATTACK_TABLE, bishopCursor);
    }
    tablesInitialized = true;
}

}