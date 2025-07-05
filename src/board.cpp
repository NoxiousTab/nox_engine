#include "../inc/board.h"
#define ui64 uint64_t
#define ui8 uint8_t
#define i16 int16_t

void Board::apply_move(const Move &move, const castling_config &castling) {
    assert(!move.isNull());
    const ui8 piece = getPieceAt(move.source);
    const ui8 pieceTyp = typeOfPiece(piece);
    const ui8 captured = getPieceAt(move.dest);

    switch(captured) {
        case Piece::None: break;
        case Piece::WhitePawn: remove_piece<Piece::WhitePawn>(move.dest); break;
        case Piece::WhiteKnight: remove_piece<Piece::WhiteKnight>(move.dest); break;
        case Piece::WhiteBishop: remove_piece<Piece::WhiteBishop>(move.dest); break;
        case Piece::WhiteRook: remove_piece<Piece::WhiteRook>(move.dest); break;
        case Piece::WhiteQueen: remove_piece<Piece::WhiteQueen>(move.dest); break;
        case Piece::BlackPawn: remove_piece<Piece::BlackPawn>(move.dest); break;
        case Piece::BlackKnight: remove_piece<Piece::BlackKnight>(move.dest); break;
        case Piece::BlackBishop: remove_piece<Piece::BlackBishop>(move.dest); break;
        case Piece::BlackRook: remove_piece<Piece::BlackRook>(move.dest); break;
        case Piece::BlackQueen: remove_piece<Piece::BlackQueen>(move.dest); break;
    }

    switch (piece) {
        case Piece::WhitePawn: remove_piece<Piece::WhitePawn>(move.source); add_piece<Piece::WhitePawn>(move.dest); break;
        case Piece::WhiteKnight: remove_piece<Piece::WhiteKnight>(move.source); add_piece<Piece::WhiteKnight>(move.dest); break;
        case Piece::WhiteBishop: remove_piece<Piece::WhiteBishop>(move.source); add_piece<Piece::WhiteBishop>(move.dest); break;
        case Piece::WhiteRook: remove_piece<Piece::WhiteRook>(move.source); add_piece<Piece::WhiteRook>(move.dest); break;
        case Piece::WhiteQueen: remove_piece<Piece::WhiteQueen>(move.source); add_piece<Piece::WhiteQueen>(move.dest); break;
        case Piece::WhiteKing: remove_piece<Piece::WhiteKing>(move.source); add_piece<Piece::WhiteKing>(move.dest); break;
        case Piece::BlackPawn: remove_piece<Piece::BlackPawn>(move.source); add_piece<Piece::BlackPawn>(move.dest); break;
        case Piece::BlackKnight: remove_piece<Piece::BlackKnight>(move.source); add_piece<Piece::BlackKnight>(move.dest); break;
        case Piece::BlackBishop: remove_piece<Piece::BlackBishop>(move.source); add_piece<Piece::BlackBishop>(move.dest); break;
        case Piece::BlackRook: remove_piece<Piece::BlackRook>(move.source); add_piece<Piece::BlackRook>(move.dest); break;
        case Piece::BlackQueen: remove_piece<Piece::BlackQueen>(move.source); add_piece<Piece::BlackQueen>(move.dest); break;
        case Piece::BlackKing: remove_piece<Piece::BlackKing>(move.source); add_piece<Piece::BlackKing>(move.dest); break;
    }

    if(move.dest == enPassantSquare) {
        if(piece == Piece::WhitePawn) {
            remove_piece<Piece::BlackPawn>(enPassantSquare - 8);
        } else if(piece == Piece::BlackPawn) {
            remove_piece<Piece::WhitePawn>(enPassantSquare + 8);
        }
    }

    if (piece == Piece::WhitePawn) {
        switch (move.flg) {
            case move_flag::none: break;
            case move_flag::promoteToQueen: remove_piece<Piece::WhitePawn>(move.dest); add_piece<Piece::WhiteQueen>(move.dest); break;
            case move_flag::promoteToKnight: remove_piece<Piece::WhitePawn>(move.dest); add_piece<Piece::WhiteKnight>(move.dest); break;
            case move_flag::promoteToRook: remove_piece<Piece::WhitePawn>(move.dest); add_piece<Piece::WhiteRook>(move.dest); break;
            case move_flag::promoteToBishop: remove_piece<Piece::WhitePawn>(move.dest); add_piece<Piece::WhiteBishop>(move.dest); break;
        }
    }
    else if (piece == Piece::BlackPawn) {
        switch (move.flg) {
            case move_flag::none: break;
            case move_flag::promoteToQueen: remove_piece<Piece::BlackPawn>(move.dest); add_piece<Piece::BlackQueen>(move.dest); break;
            case move_flag::promoteToKnight: remove_piece<Piece::BlackPawn>(move.dest); add_piece<Piece::BlackKnight>(move.dest); break;
            case move_flag::promoteToRook: remove_piece<Piece::BlackPawn>(move.dest); add_piece<Piece::BlackRook>(move.dest); break;
            case move_flag::promoteToBishop: remove_piece<Piece::BlackPawn>(move.dest); add_piece<Piece::BlackBishop>(move.dest); break;
        }
    }
    
    if (piece == Piece::WhiteKing && captured == Piece::WhiteRook) {
        if (wCanCastleShort) {
            wCanCastleShort = false;
            board_hash ^= zobrist.Castling[0];
            }
        if (wCanCastleLong) {
            wCanCastleLong = false;
            board_hash ^= zobrist.Castling[1];
        }

        if (move.flg == move_flag::castleShort) {
            remove_piece<Piece::WhiteKing>(move.dest);
            add_piece<Piece::WhiteKing>(Squares::G1);
            add_piece<Piece::WhiteRook>(Squares::F1);
        }
        else {
            remove_piece<Piece::WhiteKing>(move.dest);
            add_piece<Piece::WhiteKing>(Squares::C1);
            add_piece<Piece::WhiteRook>(Squares::D1);
        }

    }

    else if (piece == Piece::BlackKing && captured == Piece::BlackRook) {
        if (bCanCastleShort) {
            bCanCastleShort = false;
            board_hash ^= zobrist.Castling[2];
        }
        if (bCanCastleLong) {
            bCanCastleLong = false;
            board_hash ^= zobrist.Castling[3];
        }

        if (move.flg == move_flag::castleShort) {
            remove_piece<Piece::BlackKing>(move.dest);
            add_piece<Piece::BlackKing>(Squares::G8);
            add_piece<Piece::BlackRook>(Squares::F8);
        }
        else {
            remove_piece<Piece::BlackKing>(move.dest);
            add_piece<Piece::BlackKing>(Squares::C8);
            add_piece<Piece::BlackRook>(Squares::D8);
        }
    }

    if (piece == Piece::WhiteKing) {
        setWhiteShortCastleRight<false>();
        setWhiteLongCastleRight<false>();
    }
    else if (piece == Piece::BlackKing) {
        setBlackShortCastleRight<false>();
        setBlackLongCastleRight<false>();
    }
    else if (piece == Piece::WhiteRook) {
        if (move.source == castling.wShort) setWhiteShortCastleRight<false>();
        else if (move.source == castling.wLong) setWhiteLongCastleRight<false>();
    }
    else if (piece == Piece::BlackRook) {
        if (move.source == castling.bShort) setBlackShortCastleRight<false>();
        else if (move.source == castling.bLong) setBlackLongCastleRight<false>();
    }

    if (captured == Piece::WhiteRook) {
        if (move.dest == castling.wShort) setWhiteShortCastleRight<false>();
        else if (move.dest == castling.wLong) setWhiteLongCastleRight<false>();
    }
    else if (captured == Piece::BlackRook) {
        if (move.dest == castling.bShort) setBlackShortCastleRight<false>();
        else if (move.dest == castling.bLong) setBlackLongCastleRight<false>();
    }

    if (enPassantSquare != -1) {
        board_hash ^= zobrist.EnPassant[getFile(enPassantSquare)];
        enPassantSquare = -1;
    }

    if (move.flg == move_flag::enPassantPossible) {
        if (turn == side::White) {
            const bool pawnOnLeft = getFile(move.dest) != 0 && getPieceAt(move.dest - 1) == Piece::BlackPawn;
            const bool pawnOnRight = getFile(move.dest) != 7 && getPieceAt(move.dest + 1) == Piece::BlackPawn;
            if (pawnOnLeft || pawnOnRight) {
                enPassantSquare = move.dest - 8;
                board_hash ^= zobrist.EnPassant[getFile(enPassantSquare)];
            }
        }
        else {
            const bool pawnOnLeft = getFile(move.dest) != 0 && getPieceAt(move.dest - 1) == Piece::WhitePawn;
            const bool pawnOnRight = getFile(move.dest) != 7 && getPieceAt(move.dest + 1) == Piece::WhitePawn;
            if (pawnOnLeft || pawnOnRight) {
                enPassantSquare = move.dest + 8;
                board_hash ^= zobrist.EnPassant[getFile(enPassantSquare)];
            }
        }
    }

    halfMoveClk += 1;
    if (captured != Piece::None || pieceTyp == Type::Pawn) halfMoveClk = 0;
    turn = !turn;
    board_hash ^= zobrist.SideToMove;
    if (turn == side::White) fullMoveClk += 1;

    assert(popCount(wKing) == 1 && popCount(bKing) == 1);
}

ui64 Board::calcMaterialHash() const {
    ui64 materialHash = 0;
    materialHash |= static_cast<ui64>(popCount(wPawn));
    materialHash |= static_cast<ui64>(popCount(wKnight)) << 6;
    materialHash |= static_cast<ui64>(popCount(wBishop)) << 12;
    materialHash |= static_cast<ui64>(popCount(wRook)) << 18;
    materialHash |= static_cast<ui64>(popCount(wQueen)) << 24;
    materialHash |= static_cast<ui64>(popCount(bPawn)) << 30;
    materialHash |= static_cast<ui64>(popCount(bKnight)) << 36;
    materialHash |= static_cast<ui64>(popCount(bBishop)) << 42;
    materialHash |= static_cast<ui64>(popCount(bRook)) << 48;
    materialHash |= static_cast<ui64>(popCount(bQueen)) << 54;
    return murmurHash3(materialHash);
}

ui64 Board::calcPawnHash() const {
    return murmurHash3(wPawn) ^ murmurHash3(bPawn ^ zobrist.SideToMove);
};
