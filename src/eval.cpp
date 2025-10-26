#include "eval.h"
#include <array>
#include <cctype>
#include "nnue.h"

namespace eng {

static int pieceVal(char p){
    switch(p){
        case 'P': return 100; case 'N': return 320; case 'B': return 330; case 'R': return 500; case 'Q': return 900; case 'K': return 0;
        case 'p': return -100; case 'n': return -320; case 'b': return -330; case 'r': return -500; case 'q': return -900; case 'k': return 0;
        default: return 0;
    }
}

static int mirror64(int i){ return i ^ 56; }

static const std::array<int,64> PST_P = {
     0,0,0,0,0,0,0,0,
     50,50,50,50,50,50,50,50,
     10,10,20,30,30,20,10,10,
     5,5,10,25,25,10,5,5,
     0,0,0,20,20,0,0,0,
     5,-5,-10,0,0,-10,-5,5,
     5,10,10,-20,-20,10,10,5,
     0,0,0,0,0,0,0,0
};

static const std::array<int,64> PST_N = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,0,0,0,0,-20,-40,
    -30,0,10,15,15,10,0,-30,
    -30,5,15,20,20,15,5,-30,
    -30,0,15,20,20,15,0,-30,
    -30,5,10,15,15,10,5,-30,
    -40,-20,0,5,5,0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};

static const std::array<int,64> PST_B = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,0,0,0,0,0,0,-10,
    -10,0,5,10,10,5,0,-10,
    -10,5,5,10,10,5,5,-10,
    -10,0,10,10,10,10,0,-10,
    -10,10,10,10,10,10,10,-10,
    -10,5,0,0,0,0,5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};

static const std::array<int,64> PST_R = {
     0,0,0,5,5,0,0,0,
    -5,0,0,0,0,0,0,-5,
    -5,0,0,0,0,0,0,-5,
    -5,0,0,0,0,0,0,-5,
    -5,0,0,0,0,0,0,-5,
    -5,0,0,0,0,0,0,-5,
     5,10,10,10,10,10,10,5,
     0,0,0,0,0,0,0,0
};

static const std::array<int,64> PST_Q = {
    -20,-10,-10,-5,-5,-10,-10,-20,
    -10,0,0,0,0,0,0,-10,
    -10,0,5,5,5,5,0,-10,
    -5,0,5,5,5,5,0,-5,
     0,0,5,5,5,5,0,-5,
    -10,5,5,5,5,5,0,-10,
    -10,0,5,0,0,0,0,-10,
    -20,-10,-10,-5,-5,-10,-10,-20
};

static const std::array<int,64> PST_K = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20,20,0,0,0,0,20,20,
     20,30,10,0,0,10,30,20
};

static int pst(char p, int sq){
    switch(p){
        case 'P': return PST_P[sq]; case 'p': return -PST_P[mirror64(sq)];
        case 'N': return PST_N[sq]; case 'n': return -PST_N[mirror64(sq)];
        case 'B': return PST_B[sq]; case 'b': return -PST_B[mirror64(sq)];
        case 'R': return PST_R[sq]; case 'r': return -PST_R[mirror64(sq)];
        case 'Q': return PST_Q[sq]; case 'q': return -PST_Q[mirror64(sq)];
        case 'K': return PST_K[sq]; case 'k': return -PST_K[mirror64(sq)];
    }
    return 0;
}

int Eval::evaluate(const Board& b){
    if(NNUE::isEnabled() && NNUE::isReady()){
        return NNUE::evaluate(b);
    }
    const auto& brd = b.st.board;
    int score=0;
    int wB=0,bB=0,wR=0,bR=0; // counts for bishop pair and rook features
    // base material + PST
    for(int i=0;i<64;i++){
        char p = brd[i]; if(p=='.') continue;
        score += pieceVal(p);
        score += pst(p,i);
        if(p=='B') wB++; else if(p=='b') bB++;
        else if(p=='R') wR++; else if(p=='r') bR++;
    }

    // pawn structure
    auto fileOf = [](int sq){ return sq%8; };
    auto rankOf = [](int sq){ return sq/8; };
    // track pawns by files
    int wpawnFile[8] = {0}, bpawnFile[8] = {0};
    for(int i=0;i<64;i++){
        if(brd[i]=='P') wpawnFile[fileOf(i)]++;
        else if(brd[i]=='p') bpawnFile[fileOf(i)]++;
    }
    for(int i=0;i<64;i++){
        char p = brd[i]; if(p!='P' && p!='p') continue;
        int f=fileOf(i), r=rankOf(i);
        bool white = (p=='P');
        // doubled
        if(white && wpawnFile[f]>1) score -= 10;
        if(!white && bpawnFile[f]>1) score += 10;
        // isolated
        bool hasAdjSame = false;
        for(int df=-1; df<=1; df+=2){ int nf=f+df; if(nf<0||nf>7) continue; if(white){ if(wpawnFile[nf]>0) hasAdjSame=true; } else { if(bpawnFile[nf]>0) hasAdjSame=true; } }
        if(!hasAdjSame){ if(white) score -= 15; else score += 15; }
        // passed pawn
        bool passed = true;
        for(int df=-1; df<=1; ++df){ int nf=f+df; if(nf<0||nf>7) continue; if(white){
                for(int rr=r+1; rr<8; ++rr){ if(brd[rr*8+nf]=='p'){ passed=false; break; } } }
            else { for(int rr=r-1; rr>=0; --rr){ if(brd[rr*8+nf]=='P'){ passed=false; break; } } }
            if(!passed) break;
        }
        if(passed){ if(white) score += 20 + r*2; else score -= 20 + (7-r)*2; }
    }

    // mobility: count legal moves per side (cheap proxy)
    // Note: uses generateLegalMoves (may be heavier but acceptable for now)
    {
        Board copy=b; char sideSave=copy.st.side;
        copy.st.side='w'; int wmob=(int)copy.generateLegalMoves().size();
        copy.st.side='b'; int bmob=(int)copy.generateLegalMoves().size();
        // phase scaling: more weight in middlegame
        int phase=0; // 0..24 approx by non-pawn material
        for(int i=0;i<64;i++){ char p=brd[i]; switch(p){ case 'N':case 'n': case 'B':case 'b': phase+=1; break; case 'R':case 'r': phase+=2; break; case 'Q':case 'q': phase+=4; break; default: break; } }
        if(phase>24) phase=24;
        int mobWeight = 1 + phase/8; // 1..4
        score += (wmob - bmob) * mobWeight;
        copy.st.side=sideSave;
    }

    // king safety: pawn shield in front of king (opening-ish)
    int wk = b.st.wk, bk = b.st.bk;
    if(wk!=-1){ int wr = rankOf(wk); int wf=fileOf(wk); if(wr<=1){ for(int df=-1; df<=1; ++df){ int f=wf+df; if(f<0||f>7) continue; int sq = (wr+1)*8+f; if(brd[sq]=='P') score += 5; else score -= 5; } } }
    if(bk!=-1){ int br = rankOf(bk); int bf=fileOf(bk); if(br>=6){ for(int df=-1; df<=1; ++df){ int f=bf+df; if(f<0||f>7) continue; int sq = (br-1)*8+f; if(brd[sq]=='p') score -= 5; else score += 5; } } }

    // bishop pair bonus
    if(wB>=2) score += 30; if(bB>=2) score -= 30;

    // rook features: open/semi-open files and 7th rank
    auto isFileOpen = [&](int f){ for(int r=0;r<8;r++){ char p=brd[r*8+f]; if(p=='P'||p=='p') return false; } return true; };
    auto isFileSemiOpenW = [&](int f){ bool seenW=false, seenB=false; for(int r=0;r<8;r++){ char p=brd[r*8+f]; if(p=='P') seenW=true; if(p=='p') seenB=true; } return !seenW && seenB; };
    auto isFileSemiOpenB = [&](int f){ bool seenW=false, seenB=false; for(int r=0;r<8;r++){ char p=brd[r*8+f]; if(p=='P') seenW=true; if(p=='p') seenB=true; } return seenW && !seenB; };
    for(int i=0;i<64;i++){
        char p=brd[i]; if(p=='R'){ int f=fileOf(i), r=rankOf(i); if(isFileOpen(f)) score += 15; else if(isFileSemiOpenW(f)) score += 8; if(r==6) score += 15; }
        else if(p=='r'){ int f=fileOf(i), r=rankOf(i); if(isFileOpen(f)) score -= 15; else if(isFileSemiOpenB(f)) score -= 8; if(r==1) score -= 15; }
    }

    // tempo
    if(b.st.side=='w') score += 10; else score -= 10;
    return score;
}

} // namespace eng
