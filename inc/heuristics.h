#pragma once
#define ui8 uint8_t
#define i16 int16_t
#define i32 int32_t
#include "move.h"
#include "position.h"
#include "utils.h"
#include <memory>

constexpr bool bonus = 1;
constexpr bool penalty = 0;

class history {
    public:
    history();
    void clrAll();
    void clrPrevBlocks();

    void setKill(const Move &mov, const int level);
    void setCounter(const Move &prev, const Move &mov);
    void setPositional(const position &pos, const Move &mov);
    std::tuple<Move, Move, Move> getBlockedMoves(const position &pos, const int level) const;
    void resetKillPly(const int level);

    template <bool bonus> void updateQuiet(const position &pos, const Move &mov, const int level, const int depth, const int times);
    template <bool bonus> void updateCapture(const position &pos, const Move &mov, const int level, const int depth, const int times);

    int getPrevScore(const position &pos, const Move &mov, const ui8 prevMove, const int level) const;
    int getPrevCaptureScore(const position &pos, const Move &mov) const;

    void updateCorrectly(const position &pos, const i16 refEval, const i16 score, const int depth);
    i16 applyCorrectly(const position &pos, const i16 rawEval) const;

    private:
    inline void updatePrevValue(i16 &val, const int amount) {
        const int offset = val * std::abs(amount)/14900;
        val += (amount - offset);
    }

    std::array<Move, max_depth> killers;
    MultiArray<Move, 64, 64> counters;
    MultiArray<Move, 2, 8192> positionals;

    MultiArray<i16, 15, 64, 2, 2> quietHist;
    MultiArray<i16, 15, 64, 15, 2, 2> captureHist;
    MultiArray<i16, 15, 64, 15, 64> continuationHist;

    MultiArray<i32, 2, 16384> pawnCorrection;
    MultiArray<i32, 2, 2, 65536> nonPawnCorrection;
    MultiArray<i32, 15, 64, 15, 64> followUp;

};