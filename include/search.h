#pragma once
#include <atomic>
#include <optional>
#include <array>
#include <chrono>
#include <mutex>
#include "bboard.h"
#include "tt.h"

namespace eng {

struct SearchResult {
    int score{0};
    Move best{};
};

class Searcher {
public:
    // This is a root safety ceiling, not the normal stopping mechanism -- in
    // ordinary time-based play (go wtime/btime, no explicit "depth"), the
    // deadline/softDeadline checks in search() are what actually stop
    // iterative deepening; this cap should essentially never bind. It used
    // to default to 10, which DID bind on every real-time-control game
    // (nothing else in main.cpp/uci.cpp ever raised it for the common case
    // of no explicit "depth"), silently discarding the vast majority of the
    // clock in anything slower than bullet. 80 leaves 48 plies of headroom
    // under MAX_PLY (128, the killers[]/history[] array bound below) for the
    // check-extension in searchRec() and quiescence's own capture/check
    // chain to stack on top of the root depth without risking an
    // out-of-bounds killers[ply] access (see the ply-clamping added at its
    // call sites for the same reason). An explicit "go depth N" or the
    // "Skill" handicap option still override this via searcher.maxDepth as
    // before.
    int maxDepth{80};

    std::atomic<bool> stop{false};
    int contempt{0}; // centipawns bias for drawish positions
    TT tt;
    std::atomic<size_t> nodes{0};
    std::chrono::steady_clock::time_point deadline;
    std::chrono::steady_clock::time_point softDeadline;
    int threads{1};
    std::atomic<bool> parallelRoot{false};

    SearchResult search(BBoard& b, int timeMs = 1000);
    void clearForNewGame(); 

private:
    static constexpr int MAX_PLY = 128;
    std::array<std::array<Move,2>, MAX_PLY> killers{}; // two killer moves per ply
    std::array<std::array<std::array<int,64>,64>, 2> history{}; // [side][from][to], the standard history heuristic keys on the full move (from AND to), not just the origin square. Two different moves off the same square (e.g. a queen retreat vs. a queen fork) have nothing in common positionally, so collapsing them into one bucket was actively degrading move ordering.
    std::mutex khMutex; // protects killers/history updates when threaded

    int quiesce(BBoard& b, int alpha, int beta, int ply, int checksLeft = 1);
    int searchRec(BBoard& b, int depth, int alpha, int beta, int ply);
    int evalWithContempt(const BBoard& b) const;
    std::string buildPV(BBoard& b, int maxLen = 40);
    inline bool timeUp() const { return std::chrono::steady_clock::now() >= deadline; }
    inline bool timeUpSoft() const { return std::chrono::steady_clock::now() >= softDeadline; }
    bool badCaptureHeuristic(const BBoard& b, const Move& m, int stand) const;
};

}