#pragma once
#define ui8 uint8_t
#define ui16 uint16_t
#define ui64 uint64_t
#include "utils.h"


class Move {
    public:
    ui8 source;
    ui8 dest;
    ui8 flg;

    Move() : Move(0, 0, 0) {}

    Move(const ui8 source, const ui8 dest) : Move(source, dest, 0) {}
    Move(const ui8 source, const ui8 dest, const ui8 flg) {
        this->source = source;
        this->dest = dest;
        this->flg = flg;
    }

    Move(const ui16 mov) {
        source = (mov & 0xFC00) >> 10;
        dest = (mov & 0x03F0) >> 4;
        flg = mov & 0x000F;
    }

    std::string toString(const bool frc) const {

                if (source == 0 && dest == 0) {
                    return "0000";
                }

                if (!frc) {
                        if (flg == move_flag::castleShort) {
                                const bool side = source < 32 ? side::White : side::Black;
                                return side == side::White ? "e1g1" : "e8g8";
                        }
                        else if (flg == move_flag::castleLong) {
                                const bool side = source < 32 ? side::White : side::Black;
                                return side == side::White ? "e1c1" : "e8c8";
                        }
                }

                const uint8_t file1 = source % 8;
                const uint8_t rank1 = source / 8;
                const uint8_t file2 = dest % 8;
                const uint8_t rank2 = dest / 8;

                const char f1 = 'a' + file1;
                const char r1 = '1' + rank1;
                const char f2 = 'a' + file2;
                const char r2 = '1' + rank2;

                const char promo = [&] {
                        switch (flg) {
                        case move_flag::promoteToQueen: return 'q';
                        case move_flag::promoteToRook: return 'r';
                        case move_flag::promoteToBishop: return 'b';
                        case move_flag::promoteToKnight: return 'n';
                        default: return '\0';
                        }
                }();

                if (promo == '\0') return { f1, r1, f2, r2 };
                else return { f1, r1, f2, r2, promo };
        }

        inline bool isNull() const {
            return source == 0 && dest == 0;
        }

        inline bool UnderPromoting() const {
            return flg == move_flag::promoteToRook || flg == move_flag::promoteToBishop || flg == move_flag::promoteToKnight;
        }

        inline bool Promoting() const {
            return flg == move_flag::promoteToQueen || flg == move_flag::promoteToRook || flg == move_flag::promoteToKnight || flg == move_flag::promoteToBishop;
        }
        
        inline bool isCastling() const {
            return flg == move_flag::castleShort || move_flag::castleLong;
        }

        inline ui8 getPromotingPiece() const {
            if(flg == move_flag::promoteToQueen) {
                return Type::Queen;
            }else if(flg == move_flag::promoteToRook) {
                return Type::Rook;
            } else if(flg == move_flag::promoteToBishop) {
                return Type::Bishop;
            } else if(flg == move_flag::promoteToKnight) {
                return Type::Knight;
            } else {
                return Type::None;
            }
        }

        inline ui16 pack() const {
            return (source << 10) | (dest << 4) | flg;
        }

        inline bool operator== (const Move& mov) const {
            return (source == mov.source && flg == mov.flg && dest == mov.dest);
        }
};

static const Move nullMove {
    0,
    0,
    move_flag::none
};

struct moveAndpiece {
    Move move{};
    ui8 piece = 0;
};

struct scoredMove {
    Move move;
    int ordScore;
};

struct moveList : staticVector<scoredMove, max_move_cnt> {
    inline void pushNotScored(const Move &mov) {
        assert(cnt < max_move_cnt);
        memo[cnt] = {mov, 0};
        ++cnt;
    }

    inline void pushScored(const Move &mov, const int score) {
        assert(cnt < max_move_cnt);
        memo[cnt] = {mov, score};
        ++cnt;
    }

    inline void putScore(const std::size_t ind, const int score) {
        assert(cnt > ind);
        memo[ind].ordScore = score;
    }
};
