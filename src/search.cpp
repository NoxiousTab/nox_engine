#include "search.h"
#include "eval.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <thread>
#include <mutex>

namespace eng {

static int pieceVal(char p) {
    switch (p) {
        case 'P': return 100;
        case 'N': return 320;
        case 'B': return 330;
        case 'R': return 500;
        case 'Q': return 900;
        case 'K': return 0;
        case 'p': return -100;
        case 'n': return -320;
        case 'b': return -330;
        case 'r': return -500;
        case 'q': return -900;
        case 'k': return 0;
        default: return 0;
    }
}

// Mate scores are computed as +/-(MATE_VALUE - ply), where "ply" is distance
// from the root of the current search call. That makes them correct to
// report at the root, but unsafe to drop into the TT as-is: a transposition
// can reach the identical position at a *different* ply-from-root, and
// reusing a root-relative mate score there would misreport the mate
// distance (or corrupt alpha/beta comparisons) at that node. Standard fix:
// strip the root-relative ply out before storing (leaving a ply-independent
// "distance from this node" value), then re-add the CURRENT node's ply on
// retrieval to re-root it correctly.
static constexpr int MATE_VALUE = 100000;
static constexpr int MATE_THRESHOLD = MATE_VALUE - 1000; // generous margin over any realistic search ply

static int valueToTT(int v, int ply){
    if(v >= MATE_THRESHOLD) return v + ply;
    if(v <= -MATE_THRESHOLD) return v - ply;
    return v;
}
static int valueFromTT(int v, int ply){
    if(v >= MATE_THRESHOLD) return v - ply;
    if(v <= -MATE_THRESHOLD) return v + ply;
    return v;
}

static std::string moveToUciPV(const Move& m) {
    std::string s = sqToCoord(m.from) + sqToCoord(m.to);
    if ((m.flags & PROMOTION) && m.promo) s += (char)std::tolower(m.promo);
    return s;
}

std::string Searcher::buildPV(Board& b, int maxLen) {
    std::string out;
    Board bb = b; // copy
    for (int i = 0; i < maxLen; i++) {
        TTEntry e{};
        uint64_t key = bb.positionKey();
        if (!tt.probe(key, e)) break;
        Move m = e.best;
        if (!(m.from || m.to)) break;
        if (!bb.makeMove(m)) break;
        if (!out.empty()) out.push_back(' ');
        out += moveToUciPV(m);
    }
    return out;
}

bool Searcher::badCaptureHeuristic(const Board& b, const Move& m, int /*stand*/) const {
    // A capture is "bad" if the full static exchange evaluation shows it
    // loses material -- this used to be approximated with a shallow one-ply
    // lookahead (a duplicate of the same idea living in three different
    // files); it's now backed by Board::see(), a proper, verified recursive
    // exchange evaluation (see tests/see_test.py).
    if (!(m.flags & (CAPTURE | EN_PASSANT))) return false;
    return b.see(m) < 0;
}

static int mvv_lva(const Board& b, const Move& m) {
    const auto& brd = b.st.board;
    int cap = 0;
    if (m.flags & EN_PASSANT) cap = std::abs(pieceVal('p'));
    else if (m.flags & CAPTURE) cap = std::abs(pieceVal(brd[m.to]));
    if(m.flags & EN_PASSANT) cap = std::abs(pieceVal('p'));
    else if(m.flags & CAPTURE) cap = std::abs(pieceVal(brd[m.to]));
    int att = std::abs(pieceVal(brd[m.from]));
    return cap*10 - att;
}

void Searcher::clearForNewGame(){
    tt.clear();
    killers = {};
    history = {};
}

SearchResult Searcher::search(Board& b, int timeMs){
    stop = false;
    nodes = 0;
    auto start = std::chrono::steady_clock::now();
    deadline = start + std::chrono::milliseconds(timeMs);
    softDeadline = start + std::chrono::milliseconds((timeMs*90)/100);

    Move best{}; int bestScore = 0; int lastScore = 0;
    auto timeUpLocal = [&]{ return timeUp(); };

    int alphaRoot = -10000000, betaRoot = 10000000;
    for(int depth=1; depth<=maxDepth; ++depth){
        if(stop || timeUpLocal()) break;
        // aspiration window around lastScore
        int window = 30; // cp
        int alpha = std::max(alphaRoot, lastScore - window);
        int beta  = std::min(betaRoot, lastScore + window);
        // `alpha` above is mutated in place by the worker loop below (it
        // doubles as "the aspiration lower bound" AND "the best score found
        // so far", raised as moves are searched). The fail-low/fail-high
        // check after the loop needs the ORIGINAL, unmutated aspiration
        // bounds -- comparing against the mutated `alpha` means bestScore
        // <= alpha is satisfied by construction (alpha gets raised TO
        // bestScore), so failLow fired on essentially every iteration,
        // forcing a redundant full-width serial re-search every single
        // depth regardless of whether the fast aspiration pass actually
        // failed.
        const int aspAlpha = alpha, aspBeta = beta;
        // Root move generation and ordering
        auto moves = b.generateLegalMoves();
        if(moves.empty()){
            // no legal move: mate or stalemate
            best = Move{}; bestScore = 0; break;
        }
        TTEntry e{}; Move ttMove = {}; uint64_t keyRoot = b.positionKey(); if(tt.probe(keyRoot, e)) ttMove = e.best;
        std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& bmv){
            auto scoreMove = [&](const Move& m){
                int s=0;
                if(m.from==ttMove.from && m.to==ttMove.to) s+=1'000'000;
                if(m.flags&(CAPTURE|EN_PASSANT|PROMOTION)){
                    s+=100'000 + mvv_lva(b,m);
                    if(badCaptureHeuristic(b,m,0)) s -= 50'000;
                }
                return s;
            };
            return scoreMove(a) > scoreMove(bmv);
        });

        std::atomic<int> idx{0};
        std::mutex mtx;
        int localBestScore = -10000000; Move localBest{};

        auto worker = [&](){
            // Each thread works on moves
            for(;;){
                int i = idx.fetch_add(1);
                if(i >= (int)moves.size() || stop || timeUpLocal()) break;
                const Move m = moves[i];
                Board tb = b; // thread-local copy
                if(!tb.makeMove(m)) continue;
                int nextDepth = depth - 1;
                int aSnap;
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    aSnap = alpha;
                }
                int score = -searchRec(tb, nextDepth, -beta, -aSnap, 1);
                if(stop || timeUpLocal()){
                    // searchRec bails out mid-recursion by returning a bare 0
                    // once the clock runs out -- that 0 is not a real
                    // evaluation of this move, just an abort signal. Trusting
                    // it here would let a truncated, meaningless score
                    // compete against (and often beat, since localBestScore
                    // starts very negative) the fully-searched moves from
                    // this same iteration or previous depths. Discard it.
                    break;
                }
                std::lock_guard<std::mutex> lock(mtx);
                if(score > localBestScore){ localBestScore = score; localBest = m; }
                if(score > alpha){ alpha = score; }
                if(alpha >= beta){ stop = stop || timeUpLocal(); break; }
            }
        };

        if(threads > 1){
            std::vector<std::thread> pool; pool.reserve(threads);
            for(int t=0; t<threads; ++t) pool.emplace_back(worker);
            for(auto& th : pool) th.join();
        } else {
            worker();
        }

        // localBest/localBestScore unconditionally track the best move actually
        // found this depth (regardless of whether it beat the narrow aspiration
        // window) -- best/bestScore must always match that, never be left
        // pointing at a stale move from an earlier, shallower depth. Only skip
        // this if literally no move was evaluated yet this depth (e.g. time ran
        // out immediately), in which case the previous depth's result is kept.
        if(localBest.from || localBest.to){
            bestScore = localBestScore;
            best = localBest;
            lastScore = bestScore;
        }
        // Aspiration fail-low/high handling: widen window and redo serial root if needed
        bool failLow  = bestScore <= aspAlpha;
        bool failHigh = bestScore >= aspBeta;
        if((failLow || failHigh) && !stop && !timeUpLocal()){
            int widen = failHigh ? 200 : 200; // widen both sides generously
            int a2 = -10000000, b2 = 10000000;
            if(failLow){ a2 = -10000000; b2 = alpha + widen; }
            if(failHigh){ a2 = beta - widen; b2 = 10000000; }
            Move best2{}; int bs2 = -10000000;
            // Serial re-search with widened window
            for(size_t i=0;i<moves.size() && !stop && !timeUpLocal(); ++i){
                const Move m = moves[i];
                Board tb = b;
                if(!tb.makeMove(m)) continue;
                int score;
                int nextDepth = depth - 1;
                if(i==0) score = -searchRec(tb, nextDepth, -b2, -a2, 1);
                else {
                    score = -searchRec(tb, nextDepth, -a2-1, -a2, 1);
                    if(score > a2 && !stop){ score = -searchRec(tb, nextDepth, -b2, -a2, 1); }
                }
                // Same abort-signal issue as the parallel worker above: if the
                // clock ran out inside searchRec, `score` is a meaningless 0,
                // not a real evaluation. Don't let it compete for best2/bs2.
                if(stop || timeUpLocal()) break;
                if(score > bs2){ bs2 = score; best2 = m; }
                if(score > a2){ a2 = score; best2 = m; }
                if(a2 >= b2) break;
            }
            if(best2.from||best2.to){ best = best2; bestScore = bs2; lastScore = bs2; }
        }
        // searchRec() populates the TT for every node it visits at ply>=1,
        // but the root's own move loop above lives entirely in local
        // variables and never writes the root position itself into the TT.
        // buildPV() below walks the PV by repeatedly probing the TT starting
        // from the CURRENT position, so without this store its very first
        // probe (on the root) misses and the PV comes back empty every time,
        // regardless of search depth or time pressure.
        if(best.from || best.to){
            uint64_t keyRootStore = b.positionKey();
            tt.store(keyRootStore, depth, valueToTT(bestScore, 0), Bound::Exact, best);
        }
        auto now = std::chrono::steady_clock::now();
        int elapsed = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now-start).count();
        long long nps = elapsed>0 ? (long long)nodes * 1000LL / elapsed : 0LL;
        std::string pv = buildPV(b);
        std::cout << "info depth "<<depth
                  << " score cp "<<bestScore
                  << " time "<<elapsed
                  << " nodes "<<nodes
                  << " nps "<<nps
                  << (pv.empty()? "" : std::string(" pv ")+pv)
                  << std::endl;
        // Allow completing this depth; stop before starting next one on soft time
        if(timeUpLocal() || (timeUpSoft() && depth>=3)) break;
    }
    SearchResult res; res.score = bestScore; res.best = best; return res;
}

int Searcher::searchRec(Board& b, int depth, int alpha, int beta, int ply){
    if(stop || timeUp()) { stop = true; return 0; }
    ++nodes;
    // draw checks
    if(b.isDrawBy50() || b.repetitionCount() >= 3) return contempt; // drawish bias
    if(depth==0) return quiesce(b, alpha, beta, ply);

    // Detect if side to move is currently in check at this node
    char sideNow = b.st.side; 
    int ksqNow = (sideNow=='w')? b.st.wk : b.st.bk; 
    bool inCheckNow = b.squareAttacked(ksqNow, (sideNow=='w')?'b':'w');

    uint64_t key = b.positionKey();
    TTEntry e{};
    if(tt.probe(key, e) && e.depth >= depth){
        int ttScore = valueFromTT(e.score, ply);
        if(e.bound == (uint8_t)Bound::Exact) return ttScore;
        if(e.bound == (uint8_t)Bound::Lower && ttScore > alpha) alpha = ttScore;
        else if(e.bound == (uint8_t)Bound::Upper && ttScore < beta) beta = ttScore;
        if(alpha >= beta) return ttScore;
    }

    // Null-move pruning: skip when in check
    if(depth >= 3 && !inCheckNow){
        if(b.makeNullMove()){
            int R = 2 + (depth > 6); // simple reduction
            int score = -searchRec(b, depth - 1 - R, -beta, -beta + 1, ply+1);
            b.unmakeNullMove();
            if(score >= beta) return beta;
        }
    }

    auto moves = b.generateLegalMoves();
    if(moves.empty()){
        char side = b.st.side; int ksq = (side=='w')? b.st.wk : b.st.bk; bool inCheck = b.squareAttacked(ksq, (side=='w')?'b':'w');
        if(inCheck) return -100000 + ply; // mate distance
        return 0; // stalemate
    }

    // Move ordering: TT move first, then captures by MVV-LVA, then killers, then history
    Move ttMove = (tt.probe(key, e) ? e.best : Move{});
    // Snapshot killers/history under the lock exactly once, up front. This avoids
    // reading them unsynchronized inside the sort comparator (which raced against
    // the mutex-protected writes on beta cutoffs when Threads > 1), and as a side
    // benefit computes each move's score exactly once instead of the O(n log n)
    // redundant recomputation the old per-comparison lambda did.
    Move killer0{}, killer1{};
    int sideIdx = (b.st.side=='w')?0:1;
    std::vector<int> histSnapshot(moves.size());
    {
        std::lock_guard<std::mutex> lk(khMutex);
        killer0 = killers[ply][0];
        killer1 = killers[ply][1];
        for(size_t i=0;i<moves.size();++i) histSnapshot[i] = history[sideIdx][moves[i].from & 63][moves[i].to & 63];
    }
    std::vector<std::pair<int,Move>> scored; scored.reserve(moves.size());
    for(size_t i=0;i<moves.size();++i){
        const Move& m = moves[i];
        int score = 0;
        if(m.from==ttMove.from && m.to==ttMove.to && (!((m.flags & PROMOTION) && ttMove.promo && m.promo!=ttMove.promo))) score += 1'000'000;
        if(m.flags & (CAPTURE|EN_PASSANT|PROMOTION)) score += 100'000 + mvv_lva(b, m);
        if((killer0.from==m.from && killer0.to==m.to) || (killer1.from==m.from && killer1.to==m.to)) score += 50'000;
        score += histSnapshot[i];
        scored.push_back({score, m});
    }
    std::stable_sort(scored.begin(), scored.end(), [](const auto& x, const auto& y){ return x.first > y.first; });
    for(size_t i=0;i<moves.size();++i) moves[i] = scored[i].second;

    Move best = {};
    int bestScore = std::numeric_limits<int>::min();
    int origAlpha = alpha;
    int moveIndex = 0;
    bool first = true;
    for(const auto& m: moves){
        if(!b.makeMove(m)) continue;
        int nextDepth = depth - 1 + (inCheckNow ? 1 : 0); // check extension
        int score;
        bool isCapture = (m.flags & (CAPTURE|EN_PASSANT|PROMOTION));
        // Futility pruning: near leaf on quiet moves, if stand pat + margin <= alpha
        if(!inCheckNow && nextDepth == 0 && !isCapture){
            int stand = evalWithContempt(b);
            int margin = 150; // conservative
            if(stand + margin <= alpha){ b.unmakeMove(); moveIndex++; continue; }
        }
        // Light Late Move Pruning: skip very late quiet moves at low depth
        if(!inCheckNow && !isCapture && depth <= 3 && moveIndex > 12){ b.unmakeMove(); moveIndex++; continue; }
        // Prune obviously bad captures (light SEE)
        if(isCapture){ int stand = evalWithContempt(b); if(badCaptureHeuristic(b, m, stand)){ b.unmakeMove(); moveIndex++; continue; } }
        // Late Move Reductions: reduce depth for quiet, late moves
        if(!inCheckNow && nextDepth >= 2 && !isCapture && moveIndex > 3){
            int R = 1 + (moveIndex > 8);
            // Principal Variation Search (PVS)
            if(first){
                score = -searchRec(b, nextDepth, -beta, -alpha, ply+1);
                first = false;
            } else {
                score = -searchRec(b, nextDepth - R, -alpha-1, -alpha, ply+1);
                if(score > alpha){
                    score = -searchRec(b, nextDepth, -beta, -alpha, ply+1);
                }
            }
        } else {
            // Principal Variation Search: first move full window, rest zero-window
            if(first){
                score = -searchRec(b, nextDepth, -beta, -alpha, ply+1);
                first = false;
            } else {
                score = -searchRec(b, nextDepth, -alpha-1, -alpha, ply+1);
                if(score > alpha){
                    score = -searchRec(b, nextDepth, -beta, -alpha, ply+1);
                }
            }
        }
        b.unmakeMove();
        if(score >= beta){
            // store killer/history
            if(!(m.flags & (CAPTURE|EN_PASSANT|PROMOTION))){
                std::lock_guard<std::mutex> lk(khMutex);
                killers[ply][1] = killers[ply][0];
                killers[ply][0] = m;
                int sideIdx = (b.st.side=='w')?1:0; // just switched back
                history[sideIdx][m.from & 63][m.to & 63] += depth * depth;
            }
            if(tt.probe(key, e)){} // no-op
            tt.store(key, depth, valueToTT(beta, ply), Bound::Lower, m);
            return beta;
        }
        if(score > bestScore){ bestScore = score; best = m; }
        if(score > alpha){ alpha = score; best = m; }
        moveIndex++;
    }
    Bound bnd = (alpha <= origAlpha) ? Bound::Upper : (alpha >= beta ? Bound::Lower : Bound::Exact);
    tt.store(key, depth, valueToTT(alpha, ply), bnd, best);
    return alpha;
}

int Searcher::quiesce(Board& b, int alpha, int beta, int ply){
    if(stop || timeUp()) { stop = true; return alpha; }
    ++nodes;
    // If in check, search all legal evasions (no stand-pat)
    char side = b.st.side; int ksq = (side=='w')? b.st.wk : b.st.bk; bool inCheck = b.squareAttacked(ksq, (side=='w')?'b':'w');
    if(inCheck){
        auto evasions = b.generateLegalMoves();
        if(evasions.empty()) return -100000 + ply; // checkmated
        for(const auto& m: evasions){
            if(!b.makeMove(m)) continue;
            int score = -quiesce(b, -beta, -alpha, ply+1);
            b.unmakeMove();
            if(score >= beta) return beta;
            if(score > alpha) alpha = score;
        }
        return alpha;
    }

    int stand = evalWithContempt(b);
    if(stand >= beta) return beta;
    if(alpha < stand) alpha = stand;

    auto caps = b.generateCaptures();
    std::sort(caps.begin(), caps.end(), [&](const Move& m1, const Move& m2){ return mvv_lva(b,m1) > mvv_lva(b,m2); });

    for(const auto& m: caps){
        // Delta pruning: skip captures that cannot raise alpha enough
        if(m.flags & CAPTURE){
            char captured = b.st.board[m.to];
            int gain = std::abs(pieceVal(captured));
            if(stand + gain + 50 <= alpha) continue;
        }
        // Light SEE prune
        if(badCaptureHeuristic(b, m, stand)) continue;
        if(!b.makeMove(m)) continue;
        int score = -quiesce(b, -beta, -alpha, ply+1);
        b.unmakeMove();
        if(score >= beta) return beta;
        if(score > alpha) alpha = score;
    }
    return alpha;
}

int Searcher::evalWithContempt(const Board& b) const{
    int e = Eval::evaluate(b);
    // If near draw by 50-move or repetition likely, bias by contempt
    if(b.isDrawBy50() || b.repetitionCount() >= 2){
        e += (b.st.side=='w' ? contempt : -contempt);
    }
    return e;
}

}