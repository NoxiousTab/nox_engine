#pragma once
#include <cstdint>
#include <array>

namespace eng {

struct Zobrist {
    static std::array<std::array<uint64_t,64>,12> piece; // PNBRQKpnbrqk
    static std::array<uint64_t,16> castling; // 4 bits value 0..15
    static std::array<uint64_t,8> epFile; // file a..h if ep set (rank implicit)
    static uint64_t side; // black to move

    static void init();
};

} // namespace eng
