#pragma once
#include <array>
#include <string>
#include <vector>
#include "types.h"

namespace eng {

struct State {
    std::array<char,64> board{}; // '.', 'PNBRQK'/'pnbrqk'
    char side{'w'}; // 'w' or 'b'
    int castling{0}; // bit 1=K,2=Q,4=k,8=q
    int ep{-1};
    int halfmove{0};
    int fullmove{1};
    int wk{-1};
    int bk{-1};
};

class Board {
public:
    State st{};

    void setStartPos();
    void setFEN(const std::string& fen);
    std::string getFEN() const;
    uint64_t positionKey() const; // Zobrist-like key
    int repetitionCount() const;  // occurrences of current position in history
    bool isDrawBy50() const { return st.halfmove >= 100; }

    static bool inBounds(Square s) { return s >= 0 && s < 64; }
    static char colorOf(char p) { return p=='.'? 0 : (p>='A'&&p<='Z'? 'w':'b'); }

    bool squareAttacked(Square sq, char bySide) const;

    bool makeMove(const Move& m);
    void unmakeMove();
    bool makeNullMove();
    void unmakeNullMove();

    std::vector<Move> generateLegalMoves();
    std::vector<Move> generateCaptures();
    void computePins(char side, int pinnedDir[64]) const;
    int see(const Move& m) const; // static exchange evaluation in centipawns

private:
    struct Undo {
        Move m;
        char captured{'.'};
        int castling{0};
        int ep{-1};
        int halfmove{0};
        int fullmove{1};
        int wk{-1};
        int bk{-1};
        char movedFrom{'.'};
        char movedTo{'.'};
        uint64_t keyBefore{0};
        bool isNull{false};
    };

    std::vector<Undo> stack;

    // helpers
    void genPawn(Square s, char p, std::vector<Move>& out) const;
    void genSlides(Square s, char p, const int* dirs, int ndirs, std::vector<Move>& out) const;
    void genKing(Square s, char p, std::vector<Move>& out) const;
    void genCastles(Square ksq, char k, std::vector<Move>& out) const;
    bool inKnightBounds(Square from, Square to) const;
    bool slideOk(Square from, Square to, int d) const;
    bool kingStepOk(Square from, Square to) const;
};

} // namespace eng
