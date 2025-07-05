#pragma once
#define i16 int16_t
#define ui8 uint8_t
#define ui64 uint64_t
#include "position.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <immintrin.h>
#include <iterator>
#include <memory>
#include <optional>



#define NET_NAME = "nnue-eval.bin"

constexpr int featureSize = 768;
constexpr int hiddenSize = 1600;
constexpr int scale = 400;
constexpr int qA = 255;
constexpr int qB = 64;

constexpr int inputBuckCount = 14;
constexpr int outputBuckCount = 8;
constexpr std::array<int, 32> inputBuckMap = {0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 9, 9, 10, 10, 11, 11, 10, 10, 11, 11, 12, 12, 13, 13, 12, 12, 13, 13, 12, 12, 13, 13, };

struct alignas(64) networkRepresentation {
    alignas(64) MultiArray<i16, inputBuckCount, featureSize, hiddenSize> featureWeights;
    alignas(64) MultiArray<i16, outputBuckCount, hiddenSize*2> outputWeights;
    alignas(64) MultiArray<i16, hiddenSize> featureBias;
    MultiArray<i16, outputBuckCount> outputBias;
};

extern const networkRepresentation *network;

struct pieceAndSquare {
    ui8 piece;
    ui8 square;
};

struct accRepresent;
i16 neuralEval(const position &pos);
i16 neuralEval(const position &pos, const accRepresent &acc);
void loadDefaultNet();

inline int getInputBuck(const ui8 kingSq, const bool sid) {
    const ui8 transform = (sid == side::White ? 0 : 56);
    const ui8 rank = getRank(kingSq ^ transform);
    const ui8 file = (getFile(kingSq ^ transform) < 4 ? getFile(kingSq ^ transform) : (getFile(kingSq ^ transform) ^ 7));
    return inputBuckMap[4 * rank + file];
}

inline int getOutputBuck(const int pieceCnt) {
    constexpr int div = (31 + outputBuckCount)/outputBuckCount;
    return (pieceCnt - 2)/div;
}

inline bool requiresRefresh(const ui8 piece, const Move &mov, const bool sid) {
    if((sid == side::White && piece != Piece::WhiteKing) || (sid == side::Black && piece != Piece::BlackKing)) {
        return false;
    }

    const ui8 src = mov.source;
    const ui8 dest = [&] {
        if(!mov.isCastling()) {
            return mov.dest;
        }
        if(sid == side::White) {
            return (mov.flg == move_flag::castleShort) ? Squares::G1 : Squares::C1;
        }else {
            return (mov.flg == move_flag::castleShort) ? Squares::G8 : Squares::C8;
        }
    }();
    if((getFile(src) < 4) != (getFile(dest) < 4)) {
        return true;
    }
    if(getInputBuck(src, sid) != getInputBuck(dest, sid)) {
        return true;
    }
    return false;
}

struct alignas(64) accRepresent {
    std::array<std::array<i16, hiddenSize>, 2> accumulator;
    std::array<ui8, 2> activeBuck;
    std::array<ui8, 2> kingSquare;
    std::array<bool, 2> right;

    Move mov;
    ui8 moved, captured;

    void refreshAll(const position &pos) {
        refresh(side::White, pos.currState());
        refresh(side::Black, pos.currState());
    }

    void refresh(const bool sid, const Board &bord) {
        for(int i = 0; i < hiddenSize; i++) {
            accumulator[sid][i] = network->featureBias[i];
        }
        kingSquare[sid] = lsbSquare(sid == side::White ? bord.wKing : bord.bKing);
        activeBuck[sid] = getInputBuck(kingSquare[sid], sid);

        ui64 bits = bord.getOcc();
        while(bits) {
            const ui8 sq = popSquare(bits);
            const ui8 piece = bord.getPieceAt(sq);
            addFeature(sid, piece, sq);
        }
        right[sid] = true;
    }

    void addFeature(const bool sid, const ui8 piece, const ui8 sq) {
        const int feature = featureIndex(sid, piece, sq);
        const int buck = activeBuck[sid];
        for(int i = 0; i < hiddenSize; i++) {
            accumulator[sid][i] += network->featureWeights[buck][feature][i];
        }
    }

    void subAddFeature(const pieceAndSquare &first, const pieceAndSquare &sec, const pieceAndSquare &third, const bool sid) {
        const int buck = activeBuck[sid];
        const auto f1 = featureIndex(sid, first.piece, first.square);
        const auto f2 = featureIndex(sid, sec.piece, sec.square);
        const auto f3 = featureIndex(sid, third.piece, third.square);
        
        for(int i = 0; i < hiddenSize; i++) {
            accumulator[sid][i] += (- network->featureWeights[buck][f1][i] - network->featureWeights[buck][f2][i] + network->featureWeights[buck][f3][i]);
        }
    }


    inline int featureIndex(const bool persp, const ui8 piece, const ui8 sq) const {
        const ui8 pieceCol = colorOfPiece(piece);
        const ui8 pieceTyp = typeOfPiece(piece);
        constexpr int offSet = 384;
        
        const ui8 transform = (getFile(kingSquare[persp]) < 4) ? 0 : 7;
        const ui8 featureInd = (pieceCol == (sideToColor(persp)) ? 0 : offSet) + (pieceTyp - 1)*64 + ((persp == side::White) ? (sq ^ transform) : Mirror(sq ^ transform));

        return featureInd;
    }

};

struct alignas(64) cacheEntry {
    alignas(64) std::array<i16, hiddenSize> cachedAcc;
    std::array<ui64, 12> fBits{};

    cacheEntry() {
        for(int i = 0; i < hiddenSize; i++) {
            cachedAcc[i] = network->featureBias[i];
        }
    }
};

struct evalState {
    std::array<accRepresent, max_depth + 1> accumulatorSt;
    int curr_ind;
    MultiArray<cacheEntry, 2, 2*inputBuckCount> cache;

    inline void pushState(const position &pos, const Move mov, const ui8 moved, const ui8 captured) {
        ++curr_ind;
        accRepresent curr = accumulatorSt[curr_ind];
        curr.mov = mov;
        curr.moved = moved;
        curr.captured = captured;
        curr.right = {false, false};
        curr.kingSquare[side::White] = pos.wKingSQ();
        curr.kingSquare[side::Black] = pos.bKingSQ();
        curr.activeBuck[side::White] = getInputBuck(curr.kingSquare[side::White], side::White);
        curr.activeBuck[side::Black] = getInputBuck(curr.kingSquare[side::Black], side::Black);
    }

    inline void popState() {
        --curr_ind;
        assert(curr_ind >= 0);
    }

    inline void reset(const position &pos) {
        curr_ind = 0;
        accumulatorSt[0].refreshAll(pos);
    }

    i16 evaluate(const position &pos);
    void updateInc(const bool sid, const int accInd);
    void updateCache(const position &pos, const int accInd, const bool sid);

};