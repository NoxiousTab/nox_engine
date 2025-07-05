#pragma once

#define ui64 uint64_t
#define ui8 uint8_t
#include "move.h"
#include "utils.h"


ui64 getBishopAttacks(const ui8 sq, const ui64 occ);
ui64 getRookAttacks(const ui8 sq, const ui64 occ);
ui64 getQueenAttacks(const ui8 sq, const ui64 occ);


struct castling_config {
    ui8 wLong = 0;
    ui8 wShort = 0;
    ui8 bLong = 0;
    ui8 bShort = 0;
};

struct Board {
    Board() = default;
    ~Board() = default;

    // White piece bitboards
    ui64 wPawn = 0;
    ui64 wKnight = 0;
    ui64 wBishop = 0;
    ui64 wRook = 0;
    ui64 wQueen = 0;
    ui64 wKing = 0;

    // Black piece bitboards
    ui64 bPawn = 0;
    ui64 bKnight = 0;
    ui64 bBishop = 0;
    ui64 bRook = 0;
    ui64 bQueen = 0;
    ui64 bKing = 0;

    ui64 threats = 0;
    ui64 board_hash = 0;
    ui64 wNonPawnHash = 0;
    ui64 bNonPawnHash = 0;

    ui8 halfMoveClk = 0;
    uint16_t fullMoveClk = 0;
    int8_t enPassantSquare = -1;

    // White castling rights
    bool wCanCastleShort = false;
    bool wCanCastleLong = false;

    // Black castling rights
    bool bCanCastleShort = false;
    bool bCanCastleLong = false;

    std::array<ui8, 64> mail{};
    bool turn = side::White;

    template<const ui8 piece>
    inline void add_piece(const ui8 sq) {
        assert(isValidPiece(piece));
        assert(sq >= 0 && sq < 64);
        setBitTrue(PieceBit(piece), sq);
        mail[sq] = piece;
        board_hash ^= zobrist.PieceSquare[piece][sq];
        if constexpr (colorOfPiece(piece) == Color::White && isNotPawn(piece)) wNonPawnHash ^= zobrist.PieceSquare[piece][sq];
        if constexpr (colorOfPiece(piece) == Color::Black && isNotPawn(piece)) bNonPawnHash ^= zobrist.PieceSquare[piece][sq];
    }

    template<const ui8 piece>
    inline void remove_piece(const ui8 sq) {
        assert(isValidPiece(piece));
        assert(sq >= 0 && sq < 64);
        setBitFalse(PieceBit(piece), sq);
        mail[sq] = Piece::None;
        board_hash ^= zobrist.PieceSquare[piece][sq];
        if constexpr (colorOfPiece(piece) == Color::White && isNotPawn(piece)) wNonPawnHash ^= zobrist.PieceSquare[piece][sq];
        if constexpr (colorOfPiece(piece) == Color::Black && isNotPawn(piece)) bNonPawnHash ^= zobrist.PieceSquare[piece][sq];
    }

    inline ui8 getPieceAt(const ui8 sq) const {
        assert(sq >= 0 && sq < 64);
        assert(isValidPieceOrNone(mail[sq]));
        return mail[sq];
    }

    constexpr ui64 &PieceBit(const ui8 &piece) {
        if(piece == Piece::WhitePawn) {
            return wPawn;
        } else if(piece == Piece::WhiteKnight) {
            return wKnight;
        } else if(piece == Piece::WhiteBishop) {
            return wBishop;
        } else if(piece == Piece::WhiteRook) {
            return wRook;
        } else if(piece == Piece::WhiteQueen) {
            return wQueen;
        } else if(piece == Piece::WhiteKing) {
            return wKing;
        } else if(piece == Piece::BlackPawn) {
            return bPawn;
        } else if(piece == Piece::BlackKnight) {
            return bKnight;
        } else if(piece == Piece::BlackBishop) {
            return bBishop;
        } else if(piece == Piece::BlackRook) {
            return bRook;
        } else if(piece == Piece::BlackQueen) {
            return bQueen;
        } else if(piece == Piece::BlackKing) {
            return bKing;
        } else {
            assert(false);
            return wPawn;
        }
    }

    template<const bool state>
    inline void setWhiteShortCastleRight() {
        if(state != wCanCastleShort) {
            wCanCastleShort = state;
            board_hash ^= zobrist.Castling[0];
        }
    }

    template<const bool state>
    inline void setWhiteLongCastleRight() {
        if(state != wCanCastleLong) {
            wCanCastleLong = state;
            board_hash ^= zobrist.Castling[1];
        }
    }

    template<const bool state>
    inline void setBlackShortCastleRight() {
        if(state != bCanCastleShort) {
            bCanCastleShort = state;
            board_hash ^= zobrist.Castling[2];
        }
    }

    template<const bool state>
    inline void setBlackLongCastleRight() {
        if(state != bCanCastleLong) {
            bCanCastleLong = state;
            board_hash ^= zobrist.Castling[3];
        }
    }

    inline ui64 getOcc() const {
        return wPawn | wKnight | wBishop | wRook | wQueen | wKing | bPawn | bKnight | bBishop | bRook | bQueen | bKing;
    }

    void apply_move(const Move &move, const castling_config &castling);

    [[maybe_unused]] ui64 calcMaterialHash() const;
    ui64 calcPawnHash() const;

};

static_assert(sizeof(Board) == 208);