#pragma once
#define ui64 uint64_t
#define ui8 uint8_t
#include "heuristics.h"
#include "position.h"
#include "utils.h"
#include <algorithm>
#include <array>

class chooser {
    public:
    chooser() = default;

    void init(const moveGen movgen, const position &pos, const history hist, const Move &ttmove, const int level) {
        this->ttmove = ttmove;
        std::tie(killer, counter, positional) = hist.getBlockedMoves(pos, level);
        this->level = level;
        this->movgen = movgen;
        this->moves.clear();
        this->ind = 0;

        pos.moveGeneration(moves, movgen, Legality::PseudoLegal);
        
        for(auto &i : moves) {
            i.ordScore = getScore(pos, hist, i.move);
        }
    }

    std::pair<Move, int> get() {
        assert(hasNext());
        
        int bestScore = moves[ind].ordScore;
        int bestInd = ind;

        for(int i = ind + 1; i < static_cast<int>(moves.size()); i++) {
            if(moves[i].ordScore > bestScore) {
                bestScore = moves[i].ordScore;
                bestInd = i;
            }
        }

        std::swap(moves[bestInd], moves[ind]);
        ind += 1;

        return {moves[ind - 1].move, moves[ind - 1].ordScore};
    }

    bool hasNext() const {
        return ind < static_cast<int>(moves.size());
    }

    int ind = 0;
    static constexpr int maxQuietOrder = 200000;

    private:

    int getScore(const position &pos, const history &hist, const Move &mov) const {
        if(mov == ttmove) {
            return 900000;
        }

        constexpr std::array<int, 7> vals = {0, 100, 300, 300, 500, 900, 0};

        const ui8 moved_piece = pos.getPieceAt(mov.source);
        const ui8 captured_pieceType = [&] {
            if(mov.isCastling())return Type::None;
            if(mov.flg == move_flag::enPassantDone) return Type::Pawn;
            return typeOfPiece(pos.getPieceAt(mov.dest));
        }();
        
        if(mov.flg == move_flag::promoteToQueen) {
            return 700000 + vals[captured_pieceType];
        }

        if(captured_pieceType != Type::None) {
            const bool capturingLoses = [&] {
                if(movgen == moveGen::Noisy) {
                    return false;
                }
                if(pos.isQuiet(mov)) return false;
                const i16 capScore = (mov.Promoting()) ? 0 : hist.getPrevCaptureScore(pos, mov);
                return !pos.exchgEval(mov, -capScore/33);
            }();
            return (!capturingLoses ? 600000 : -200000) + vals[captured_pieceType] * 16 + hist.getPrevCaptureScore(pos, mov);
        }

        int histScore = hist.getPrevScore(pos, mov, moved_piece, level);

        if(mov == killer) histScore += 22000;
        else if(mov == counter) histScore += 16000;
        else if(mov == positional) histScore += 16000;

        return histScore;

    }

    Move ttmove{}, killer{}, counter{}, positional{};
    moveList moves{};
    int level = 0;
    moveGen movgen = moveGen::All;

};