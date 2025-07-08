#pragma once
#define i16 int16_t
#define ui8 uint8_t
#define ui16 uint16_t
#define ui64 uint64_t
#include "heuristics.h"
#include "chooser.h"
#include "nnue.h"
#include "position.h"
#include "report.h"
#include "tables.h"
#include "utils.h"
#include <atomic>
#include <iomanip>
#include <algorithm>
#include <condition_variable>
#include <fstream>
#include <list>
#include <mutex>
#include <random>
#include <thread>
#include <tuple>


enum class thread_action {
    sleep,
    search,
    quit
};

class alignas(64) threadContext {
    public:
    void resetStats();

    int rootD = 0; int selD = 0;
    i64 nodes = 0;
    history hist;
    MultiArray<Move, max_depth + 1, max_depth + 1> table;
    std::array<int, max_depth + 1> length;
    evalState eState;
    MultiArray<ui64, 64, 64> rootNodeCounts;

    void updateTable(const Move &mov, const int level);
    void initLength(const int level);
    std::vector<Move> generateLine() const;
    void resetTable();

    std::array<chooser, max_depth> chooserSt;
    std::array<int, max_depth> staticEvalStack;
    std::array<int, max_depth> evalStack;
    std::array<int, max_depth> cutoffCnt;
    std::array<Move, max_depth> excluded;
    std::array<bool, max_depth> supersingular;

    position currPos;

    std::thread Thread;
    int id;

    results res;
    bool singleThread = false;

    inline bool isThreadMain() const {
        return id == 0;
    }

    std::mutex Mut;
    std::condition_variable condVar;
    thread_action act;
    bool quited = false;

};

class search {
    public:
    search();
    void resetState(const bool clearT);
    void initThreads(const int count);
    void stopThreads();
    void setThreads(const int count);
    void startSearch(position &pos, const searchParameters params);
    void stopSearch();
    void loop(threadContext &t);
    results searchSingleThread(const position &pos, const searchParameters &params);
    void waitUntilReady();
    void perft(position &pos, const int depth, const perftType typ) const;

#ifdef NOX_GENERATION
    static constexpr bool genMode = true;
#else
    static constexpr bool genMode = false;
#endif

    std::atomic<bool> aborting = true;
    tables table;

    std::list<threadContext> threads;
    std::atomic<int> activeThreads = 0;
    std::atomic<int> loadedThreads = 0;

    private:
    results threadResults() const;

    void searchMoves(threadContext &t);
    template<bool pvNode> int backtrackSearch(threadContext &t, int depth, const int level, int alpha, int beta, const bool cutNode);
    template<bool pvNode> int quietSearch(threadContext &t, const int level, int alpha, int beta);

    i16 evaluate(threadContext &t, const position &pos);
    ui64 recursivePerft(position &pos, const int depth, const int ogDepth, const perftType typ) const;
    searchConstraints calcParams(const searchParameters params, const bool turn) const;
    bool shouldQuit(const threadContext &t);
    int drawEval(const threadContext &t) const;

    searchConstraints constraints;
    std::chrono::high_resolution_clock::time_point startSearchTime;
    MultiArray<int, 32, 32> lmrTable;

};