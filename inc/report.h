#pragma once

#ifdef __cpp_lib_format
#include <format>
#endif
#define ui64 uint64_t
#include "move.h"
#include "settings.h"

struct results {
    int score = 0;
    int depth = 0;
    int selDepth = 0;
    ui64 nodes = 0;
    ui64 times = 0;
    ui64 nps = 0;
    int hashfull = 0;
    int ply = 0;
    std::vector<Move> pv;
    int threads = 1;

    results();
    results(const int score, const int depth, const int selDepth, const ui64 nodes, const ui64 time, const ui64 nps, const int hashfull, const int ply, const std::vector<Move> &pv);
    Move bestMove() const;
};

void debugInfo(const results &r);
void debugPretty(const results &r);
void debugBestMove(const Move &move);