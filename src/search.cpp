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

static bool inBounds(int s){ return s>=0 && s<64; }
static bool knightStepOk(int from,int to){ if(!inBounds(to)) return false; int ff=from%8, tf=to%8; int fr=from/8, tr=to/8; int df=std::abs(ff-tf), dr=std::abs(fr-tr); return (df==1&&dr==2)||(df==2&&dr==1); }
static bool kingStepOkLocal(int from,int to){ if(!inBounds(to)) return false; int df=std::abs(from%8 - to%8), dr=std::abs(from/8 - to/8); return std::max(df,dr)==1; }
static bool slideOkLocal(int from,int to,int d){ if(!inBounds(to)) return false; int ff=from%8, tf=to%8; int fr=from/8, tr=to/8; if(d== -1 || d==1) return std::abs(tf-ff)==std::abs(to-from); if(d== -9 || d== -7 || d==7 || d==9) return std::abs(tf-ff)==std::abs(tr-fr); if(d== -8 || d==8) return tf==ff; return true; }

static int lvaAttackerValue(const Board& b, int sq, char side){
    static const int KNIGHT_DIRS[8] = {-17,-15,-10,-6,6,10,15,17};
    static const int BISHOP_DIRS[4] = {-9,-7,7,9};
    static const int ROOK_DIRS[4]   = {-8,-1,1,8};
    static const int KING_DIRS[8]   = {-9,-8,-7,-1,1,7,8,9};
    const auto& brd = b.st.board;
    int best = 1e9;
    for(int f=0; f<64; ++f){ char p = brd[f]; if(p=='.') continue; if((side=='w' && !(p>='A'&&p<='Z')) || (side=='b' && !(p>='a'&&p<='z'))) continue;
        char up = std::toupper(p);
        bool attacks=false;
        if(up=='N'){
            for(int d:KNIGHT_DIRS){ int to=f+d; if(knightStepOk(f,to) && to==sq){ attacks=true; break; } }
        } else if(up=='B' || up=='Q'){
            for(int d:BISHOP_DIRS){ int to=f+d; while(inBounds(to) && slideOkLocal(f,to,d)){ char q=brd[to]; if(to==sq){ attacks=true; break; } if(q!='.') break; to+=d; } if(attacks) break; }
        } else if(up=='R' || up=='Q'){
            for(int d:ROOK_DIRS){ int to=f+d; while(inBounds(to) && slideOkLocal(f,to,d)){ char q=brd[to]; if(to==sq){ attacks=true; break; } if(q!='.') break; to+=d; } if(attacks) break; }
        } else if(up=='K'){
            for(int d:KING_DIRS){ int to=f+d; if(kingStepOkLocal(f,to) && to==sq){ attacks=true; break; } }
        } else if(up=='P'){
            int dir = (p=='P')? 8 : -8; // white pawns attack up (sq-from == 7 or 9)
            int df1 = (p=='P')? 7 : -7; int df2 = (p=='P')? 9 : -9; if(f+df1==sq || f+df2==sq) attacks=true;
        }
        if(attacks){ int v = std::abs(pieceVal(p)); if(v < best) best = v; }
    }
    return best==1e9? 0 : best;
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
    if (!(m.flags & (CAPTURE | EN_PASSANT))) return false;
    const auto& brd = b.st.board;
    char attacker = brd[m.from];
    char captured = (m.flags & EN_PASSANT) ? (b.st.side == 'w' ? 'p' : 'P') : brd[m.to];
    int attV = std::abs(pieceVal(attacker));
    int capV = std::abs(pieceVal(captured));
    // Quick MVV-LVA gate: if we win material on face value, accept
    if (capV >= attV) return false;
    // Light square safety check: after move, is our piece attacked on the target square?
    Board tb = b; if(!tb.makeMove(m)) return true; // illegal -> bad
    char opp = (tb.st.side=='w')?'b':'w'; // side to move is opponent after makeMove
    bool attacked = tb.squareAttacked(m.to, opp);
    tb.unmakeMove();
    if(!attacked) return false;
    // Quick LVA-based exchange check: can opponent recapture cheaply?
    int oppLVA = lvaAttackerValue(tb, m.to, opp);
    if(oppLVA==0) return false; // no recapture
    // If opponent's cheapest attacker is less valuable than the gain, likely bad
    return (capV - oppLVA) < -20;
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
                std::lock_guard<std::mutex> lock(mtx);
                if(score > localBestScore){ localBestScore = score; localBest = m; }
                if(score > alpha){ alpha = score; best = m; bestScore = score; }
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

        lastScore = bestScore = (best.from||best.to) ? std::max(localBestScore, bestScore) : localBestScore;
        if(!(best.from||best.to)) best = localBest;
        // Aspiration fail-low/high handling: widen window and redo serial root if needed
        bool failLow  = bestScore <= alpha;
        bool failHigh = bestScore >= beta;
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
                if(score > bs2){ bs2 = score; best2 = m; }
                if(score > a2){ a2 = score; best2 = m; }
                if(a2 >= b2) break;
            }
            if(best2.from||best2.to){ best = best2; bestScore = bs2; lastScore = bs2; }
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
        if(e.bound == (uint8_t)Bound::Exact) return e.score;
        if(e.bound == (uint8_t)Bound::Lower && e.score > alpha) alpha = e.score;
        else if(e.bound == (uint8_t)Bound::Upper && e.score < beta) beta = e.score;
        if(alpha >= beta) return e.score;
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
    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& bmv){
        auto scoreMove = [&](const Move& m){
            int score = 0;
            if(m.from==ttMove.from && m.to==ttMove.to && (!((m.flags & PROMOTION) && ttMove.promo && m.promo!=ttMove.promo))) score += 1'000'000;
            if(m.flags & (CAPTURE|EN_PASSANT|PROMOTION)) score += 100'000 + mvv_lva(b, m);
            // killers
            for(int i=0;i<2;i++){ if(killers[ply][i].from==m.from && killers[ply][i].to==m.to) { score += 50'000; break; } }
            // history (very simple: index by from square per side)
            int sideIdx = (b.st.side=='w')?0:1;
            score += history[sideIdx][m.from & 63];
            return score;
        };
        return scoreMove(a) > scoreMove(bmv);
    });

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
                history[sideIdx][m.from & 63] += depth * depth;
            }
            if(tt.probe(key, e)){} // no-op
            tt.store(key, depth, beta, Bound::Lower, m);
            return beta;
        }
        if(score > bestScore){ bestScore = score; best = m; }
        if(score > alpha){ alpha = score; best = m; }
        moveIndex++;
    }
    Bound bnd = (alpha <= origAlpha) ? Bound::Upper : (alpha >= beta ? Bound::Lower : Bound::Exact);
    tt.store(key, depth, alpha, bnd, best);
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

} // namespace eng
