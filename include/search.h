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
    size_t nodes{0};
    std::chrono::steady_clock::time_point deadline;
    std::chrono::steady_clock::time_point softDeadline;
    int threads{1};
    std::atomic<bool> parallelRoot{false};

    SearchResult search(Board& b, int timeMs = 1000);

private:
    static constexpr int MAX_PLY = 128;
    std::array<std::array<Move,2>, MAX_PLY> killers{}; // two killer moves per ply
    std::array<std::array<int,64>, 2> history{}; // side index 0=w,1=b; from*8+to%8 simplified: use [from%64]
    std::mutex khMutex; // protects killers/history updates when threaded

    int quiesce(Board& b, int alpha, int beta, int ply);
    int searchRec(Board& b, int depth, int alpha, int beta, int ply);
    int evalWithContempt(const Board& b) const;
    std::string buildPV(Board& b, int maxLen = 40);
    inline bool timeUp() const { return std::chrono::steady_clock::now() >= deadline; }
    inline bool timeUpSoft() const { return std::chrono::steady_clock::now() >= softDeadline; }
    bool badCaptureHeuristic(const Board& b, const Move& m, int stand) const;
};

} // namespace eng
