#pragma once
#define ui64 uint64_t
#define ui16 uint16_t
#define ui8 uint8_t
#include "board.h"
#include "settings.h"
#include "move.h"
#include "utils.h"

ui64 getBishopAttacks(const ui8 sq, const ui64 occ);
ui64 getRookAttacks(const ui8 sq, const ui64 occ);
ui64 getQueenAttacks(const ui8 sq, const ui64 occ);
ui64 getConnectionLine(const ui8 src, const ui64 dest);


class position {
    public:
        position(const std::string &fen);
        position() : position(Fen::startPos) {};
        position(const int frcW, const int frcB);

        void pushMove(const Move &mov);
        void pushNull();
        void pushUCI(const std::string &str);
        void popMove();

        void moveGeneration(moveList &moves, const moveGen movGen, const Legality legality) const;
        bool isDraw(const int level) const;

        bool isLegal(const Move &mov) const;
        bool isQuiet(const Move &mov) const;

        inline Board &currState() {
            return states.back();
        }

        inline const Board &currState() const {
            return states.back();
        }

        inline ui64 getOcc() const {
            const Board &bord = states.back();
            ui64 temp = (bord.wPawn | bord.wKing | bord.wKnight | bord.wBishop | bord.wQueen | bord.wRook | bord.bPawn | bord.bKing | bord.bKnight | bord.bBishop | bord.bQueen | bord.bRook);
            return temp;
        }

        inline ui64 getOcc(const bool which) const {
            const Board &bord = states.back();
            return (which == side::White ? (bord.wPawn | bord.wKing | bord.wKnight | bord.wBishop | bord.wQueen | bord.wRook) : (bord.bPawn | bord.bKing | bord.bKnight | bord.bBishop | bord.bQueen | bord.bRook));
        }

        inline ui8 getPieceAt(const ui8 sq) const {
            return states.back().getPieceAt(sq);
        }

        inline bool turn() const {
            return states.back().turn;
        }

        inline bool inCheck() const {
            const ui8 kingAt = (turn() == side::White) ? wKingSQ() : bKingSQ();
            return isThreatened(kingAt);
        }

        inline ui64 Hash() const {
            return states.back().board_hash;
        }

        inline int getPly() const {
            const Board &bord = states.back();
            int offSet = (bord.turn == side::White ? 0 : 1);
            return ((bord.fullMoveClk - 1) * 2 + offSet);
        }

        inline bool Zugzwang() const {
            const Board &bord = states.back();
            return (bord.turn == side::White ? ((bord.wPawn | bord.wKing | bord.wKnight | bord.wBishop | bord.wQueen | bord.wRook) != 0ull) : ((bord.bPawn | bord.bKing | bord.bKnight | bord.bBishop | bord.bQueen | bord.bRook) != 0ull));
        }

        inline int getPhase() const {
            return (popCount(wKnight() | bKnight()))
                        + (popCount(wBishop() | bBishop()))
                        + (popCount(wRook() | bRook())) * 2
                        + (popCount(wQueen() | bQueen())) * 4;
        }

        template<bool sd>
        inline ui64 getPawnAttacks() const {
            const Board &bord = currState();
            if constexpr (sd == side::White) {
                return ((bord.wPawn & ~File[0] << 7) | (bord.wPawn & ~File[7] << 9));
            } else {
                return ((bord.bPawn & ~File[0] >> 7) | (bord.bPawn & ~File[7] >> 7));
            }
        }

        inline ui64 genKnightAttacks(const int src) const {
            return knightMoveBits[src];
        }

        inline ui64 genKingAttacks(const int src) const {
            return kingMoveBits[src];
        }

        inline const moveAndpiece &getPrevMove(const int plys) const {
            assert(plys > 0);
            assert(plys <= moves.size());
            return moves[moves.size() - plys];
        }

        inline bool isPrevNull() const {
            return moves.size() != 0 && moves.back().move == nullMove;
        }

        inline ui64 getThreats() const {
            return states.back().threats;
        }

        inline bool isThreatened(const ui8 sq) const {
            return checkBit(states.back().threats, sq);
        }

        inline ui64 getMatsHash() const {
            return states.back().calcMaterialHash();
        }

        inline ui64 getPawnHash() const {
            return states.back().calcPawnHash();
        }

        inline std::pair<ui64, ui64> getNonPawnHash() const {
            return {states.back().wNonPawnHash, states.back().bNonPawnHash};
        }

        inline ui64 approximateHash(const Move &mov) const {
            ui64 currHash = states.back().board_hash ^ zobrist.SideToMove;
            
            const ui8 moved = getPieceAt(mov.source);
            const ui8 captured = getPieceAt(mov.dest);
        
            currHash ^= zobrist.PieceSquare[moved][mov.source];
            currHash ^= zobrist.PieceSquare[moved][mov.dest];

            if(captured != Piece::None) {
                currHash ^= zobrist.PieceSquare[captured][mov.dest];
            }
            return currHash;
        }

        inline ui64 wPawn() const {
            return states.back().wPawn;
        }

        inline ui64 wKnight() const {
            return states.back().wKnight;
        }

        inline ui64 wBishop() const {
            return states.back().wBishop;
        }

        inline ui64 wRook() const {
            return states.back().wRook;
        }

        inline ui64 wQueen() const {
            return states.back().wQueen;
        }
        
        inline ui64 wKing() const {
            return states.back().wKing;
        }

        inline ui64 bPawn() const {
            return states.back().bPawn;
        }

        inline ui64 bKnight() const {
            return states.back().bKnight;
        }

        inline ui64 bBishop() const {
            return states.back().bBishop;
        }

        inline ui64 bRook() const {
            return states.back().bRook;
        }

        inline ui64 bQueen() const {
            return states.back().bQueen;
        }
        
        inline ui64 bKing() const {
            return states.back().bKing;
        }

        inline ui8 wKingSQ() const {
            return lsbSquare(states.back().wKing);
        }

        inline ui8 bKingSQ() const {
            return lsbSquare(states.back().bKing);
        }

        ui64 getAttackers(const ui8 sq, const ui64 occ) const;
        std::string getFen() const;
        gameState getState() const;
        bool exchgEval(const Move &mov, const int thresh) const;

        std::vector<Board> states{};
        std::vector<moveAndpiece> moves{};
        castling_config castlingConfig{};


        // move gen functs

        template <bool sid, moveGen movGen> void genPseudoLegal(moveList &moves) const;
        template <bool sid, moveGen movGen> void genKnightMoves(moveList &moves, const int from);
        template <bool sid, moveGen movGen> void genKingMoves(moveList &moves, const int from);
        template <bool sid, moveGen movGen> void genPawnMoves(moveList &moves, const int from);
        template <bool sid> void genCastlingMoves(moveList &moves) const;
        template <bool sid, int pieceType, moveGen movGen> void genSlideMoves(moveList &moves, const int from, const ui64 wOcc, const ui64 bOcc) const;

        bool isAttacked(const bool attacker, const ui8 sq, const ui64 occ) const;
        ui64 calcAttackedSquares(const bool attacker) const;
};