#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace eng {

using Square = int; // 0..63

enum MoveFlags : uint16_t {
    CAPTURE   = 1 << 0,
    EN_PASSANT= 1 << 1,
    DOUBLE_PAWN=1 << 2,
    CASTLE    = 1 << 3,
    PROMOTION = 1 << 4,
};

struct Move {
    Square from{0};
    Square to{0};
    char promo{0};
    uint16_t flags{0};
};

inline std::string sqToCoord(Square sq) {
    static const char* FILES = "abcdefgh";
    static const char* RANKS = "12345678";
    int f = sq % 8;
    int r = sq / 8;
    std::string s; s += FILES[f]; s += RANKS[r];
    return s;
}

inline Square coordToSq(const std::string& s) {
    static const std::string FILES = "abcdefgh";
    static const std::string RANKS = "12345678";
    int f = FILES.find(s[0]);
    int r = RANKS.find(s[1]);
    return r * 8 + f;
}

} // namespace eng
