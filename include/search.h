#pragma once
#include <atomic>
#include <optional>
#include <array>
#include <chrono>
#include <mutex>
#include "board.h"
#include "tt.h"

namespace eng {

struct SearchResult {
    int score{0};
    Move best{};
};

class Searcher {
public:
    int maxDepth{10};
    std::atomic<bool> stop{false};
    int contempt{0}; // centipawns bias for drawish positions
    TT tt;
    std::atomic<size_t> nodes{0};
    std::chrono::steady_clock::time_point deadline;
    std::chrono::steady_clock::time_point softDeadline;
    int threads{1};
    std::atomic<bool> parallelRoot{false};

    SearchResult search(Board& b, int timeMs = 1000);
    void clearForNewGame(); 

private:
    static constexpr int MAX_PLY = 128;
    std::array<std::array<Move,2>, MAX_PLY> killers{}; // two killer moves per ply
    std::array<std::array<std::array<int,64>,64>, 2> history{}; // [side][from][to], the standard history heuristic keys on the full move (from AND to), not just the origin square. Two different moves off the same square (e.g. a queen retreat vs. a queen fork) have nothing in common positionally, so collapsing them into one bucket was actively degrading move ordering.
    std::mutex khMutex; // protects killers/history updates when threaded

    int quiesce(Board& b, int alpha, int beta, int ply);
    int searchRec(Board& b, int depth, int alpha, int beta, int ply);
    int evalWithContempt(const Board& b) const;
    std::string buildPV(Board& b, int maxLen = 40);
    inline bool timeUp() const { return std::chrono::steady_clock::now() >= deadline; }
    inline bool timeUpSoft() const { return std::chrono::steady_clock::now() >= softDeadline; }
    bool badCaptureHeuristic(const Board& b, const Move& m, int stand) const;
};

}