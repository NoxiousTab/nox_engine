#include "board.h"
#include <cassert>
#include <cctype>
#include <sstream>
#include <cmath>
#include <algorithm>
#include "zobrist.h"

namespace eng {

static const int KNIGHT_DIRS[8] = {-17,-15,-10,-6,6,10,15,17};
static const int BISHOP_DIRS[4] = {-9,-7,7,9};
static const int ROOK_DIRS[4]   = {-8,-1,1,8};
static const int QUEEN_DIRS[8]  = {-9,-8,-7,-1,1,7,8,9};
static const int KING_DIRS[8]   = {-9,-8,-7,-1,1,7,8,9};

void Board::setStartPos(){
    setFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

int Board::see(const Move& m) const{
    // Very light SEE: face-value gain minus cheapest immediate recapture value if square remains attacked.
    const auto& brd = st.board;
    if(!(m.flags & (CAPTURE|EN_PASSANT))) return 0;
    auto absVal = [](char p){ switch(p){ case 'P':case 'p': return 100; case 'N':case 'n': return 320; case 'B':case 'b': return 330; case 'R':case 'r': return 500; case 'Q':case 'q': return 900; default: return 0; } };
    char captured = (m.flags & EN_PASSANT)? ((st.side=='w')?'p':'P') : brd[m.to];
    char attacker = brd[m.from];
    int gain = absVal(captured) - absVal(attacker);
    // Simulate move and check if target square can be recaptured cheaply
    Board tb = *this;
    if(!tb.makeMove(m)) return -10000;
    char opp = (tb.st.side=='w')?'b':'w';
    // Find least valuable attacker of the square by opponent
    int sq = m.to;
    int best = 1e9;
    static const int KNIGHT_DIRS[8] = {-17,-15,-10,-6,6,10,15,17};
    static const int BISHOP_DIRS[4] = {-9,-7,7,9};
    static const int ROOK_DIRS[4]   = {-8,-1,1,8};
    static const int KING_DIRS[8]   = {-9,-8,-7,-1,1,7,8,9};
    auto inB=[&](int s){ return s>=0 && s<64; };
    auto kOk=[&](int f,int t){ if(!inB(t)) return false; int df=std::abs(f%8 - t%8), dr=std::abs(f/8 - t/8); return std::max(df,dr)==1; };
    auto nOk=[&](int f,int t){ if(!inB(t)) return false; int ff=f%8, tf=t%8; int fr=f/8, tr=t/8; int df=std::abs(ff-tf), dr=std::abs(fr-tr); return (df==1&&dr==2)||(df==2&&dr==1); };
    auto slOk=[&](int f,int t,int d){ if(!inB(t)) return false; int ff=f%8, tf=t%8; int fr=f/8, tr=t/8; if(d== -1 || d==1) return std::abs(tf-ff)==std::abs(t-f); if(d== -9 || d== -7 || d==7 || d==9) return std::abs(tf-ff)==std::abs(tr-fr); if(d== -8 || d==8) return tf==ff; return true; };
    const auto& b2 = tb.st.board;
    for(int f=0; f<64; ++f){
        char p=b2[f]; if(p=='.') continue; if(colorOf(p)!=opp) continue; char up=std::toupper(p);
        bool att=false;
        if(up=='N'){
            for(int d:KNIGHT_DIRS){ int t=f+d; if(nOk(f,t)&&t==sq){ att=true; break; } }
        } else if(up=='B'||up=='Q'){
            for(int d:BISHOP_DIRS){
                int t=f+d;
                while(inB(t)&&slOk(f,t,d)){
                    char q=b2[t];
                    if(t==sq){ att=true; break; }
                    if(q!='.') break;
                    t+=d;
                }
                if(att) break;
            }
        } else if(up=='R'||up=='Q'){
            for(int d:ROOK_DIRS){
                int t=f+d;
                while(inB(t)&&slOk(f,t,d)){
                    char q=b2[t];
                    if(t==sq){ att=true; break; }
                    if(q!='.') break;
                    t+=d;
                }
                if(att) break;
            }
        } else if(up=='K'){
            for(int d:KING_DIRS){ int t=f+d; if(kOk(f,t)&&t==sq){ att=true; break; } }
        } else if(up=='P'){
            int df1=(p=='P')?7:-7, df2=(p=='P')?9:-9; if(f+df1==sq||f+df2==sq) att=true;
        }
        if(att){ int v=absVal(p); if(v<best) best=v; }
    }
    if(best==1e9) return gain;
    return gain - best;
}

std::vector<Move> Board::generateCaptures(){
    std::vector<Move> moves, legal;
    const auto& b = st.board; char side = st.side;
    int pinnedDir[64]; computePins(side, pinnedDir);
    for(int s=0;s<64;++s){ char p=b[s]; if(p=='.' || colorOf(p)!=side) continue;
        switch(std::toupper(p)){
            case 'P': {
                // Use genPawn but filter captures
                std::vector<Move> tmp; genPawn(s,p,tmp);
                for(const auto& m: tmp){ if(m.flags & (CAPTURE|EN_PASSANT|PROMOTION)) moves.push_back(m); }
                break;
            }
            case 'N': for(int d:KNIGHT_DIRS){ int to=s+d; if(!inKnightBounds(s,to)) continue; char q=inBounds(to)?b[to]:'.'; if(q!='.' && colorOf(q)!=colorOf(p)) moves.push_back({s,to,0,CAPTURE}); } break;
            case 'B': { std::vector<Move> tmp; genSlides(s,p,BISHOP_DIRS,4,tmp); for(const auto& m: tmp){ if(m.flags & CAPTURE) moves.push_back(m);} break; }
            case 'R': { std::vector<Move> tmp; genSlides(s,p,ROOK_DIRS,4,tmp); for(const auto& m: tmp){ if(m.flags & CAPTURE) moves.push_back(m);} break; }
            case 'Q': { std::vector<Move> tmp; genSlides(s,p,QUEEN_DIRS,8,tmp); for(const auto& m: tmp){ if(m.flags & CAPTURE) moves.push_back(m);} break; }
            case 'K': {
                std::vector<Move> tmp; genKing(s,p,tmp);
                for(const auto& m: tmp){ if(m.flags & CAPTURE) moves.push_back(m); }
                break;
            }
        }
    }
    auto alignedWith = [&](int from,int to,int d){ if(d==0) return true; int delta = to-from; if(delta==0) return true; if(delta % d != 0) return false; return slideOk(from,to,d); };
    for(const auto& m : moves){
        char pc = b[m.from]; if(std::toupper(pc)!='K' && pinnedDir[m.from]!=0){ if(!alignedWith(m.from, m.to, pinnedDir[m.from])) continue; }
        if(makeMove(m)){ legal.push_back(m); unmakeMove(); }
    }
    return legal;
}

void Board::computePins(char side, int pinnedDir[64]) const{
    for(int i=0;i<64;++i) pinnedDir[i]=0;
    int ksq = (side=='w')? st.wk : st.bk; if(ksq<0) return;
    const auto& b = st.board;
    auto scan = [&](int d, bool rookLike){
        int sq = ksq + d; int pinnedSq = -1;
        while(inBounds(sq) && slideOk(ksq, sq, d)){
            char p = b[sq];
            if(p=='.'){ sq += d; continue; }
            if(colorOf(p)==side){ if(pinnedSq==-1){ pinnedSq = sq; sq += d; continue; } else break; }
            // opponent piece
            char up = std::toupper(p);
            bool isSlider = (rookLike? (up=='R'||up=='Q') : (up=='B'||up=='Q'));
            if(isSlider && pinnedSq!=-1){ pinnedDir[pinnedSq] = d; }
            break;
        }
    };
    for(int d : ROOK_DIRS) scan(d, true);
    for(int d : BISHOP_DIRS) scan(d, false);
}

bool Board::makeNullMove(){
    // Disallow null move if in check to be safe
    char opp = (st.side=='w')?'b':'w';
    int ksq = (st.side=='w')? st.wk : st.bk;
    if(squareAttacked(ksq, opp)) return false;
    Undo u; u.isNull = true; u.castling=st.castling; u.ep=st.ep; u.halfmove=st.halfmove; u.fullmove=st.fullmove; u.wk=st.wk; u.bk=st.bk; u.keyBefore = positionKey();
    stack.push_back(u);
    // Null move: switch side, clear ep, increment fullmove if black to move was making null
    st.ep = -1;
    if(st.side=='b') st.fullmove++;
    st.side = opp;
    st.halfmove++; // per convention
    return true;
}

void Board::unmakeNullMove(){
    assert(!stack.empty());
    Undo u = stack.back(); stack.pop_back();
    // restore
    st.castling=u.castling; st.ep=u.ep; st.halfmove=u.halfmove; st.fullmove=u.fullmove; st.wk=u.wk; st.bk=u.bk; st.side = (st.side=='w')? 'b':'w';
}

static int pieceIndex(char p){
    switch(p){
        case 'P': return 0; case 'N': return 1; case 'B': return 2; case 'R': return 3; case 'Q': return 4; case 'K': return 5;
        case 'p': return 6; case 'n': return 7; case 'b': return 8; case 'r': return 9; case 'q': return 10; case 'k': return 11;
        default: return -1;
    }
}

uint64_t Board::positionKey() const{
    uint64_t h = 0;
    for(int sq=0; sq<64; ++sq){
        int idx = pieceIndex(st.board[sq]);
        if(idx >= 0) h ^= Zobrist::piece[idx][sq];
    }
    h ^= Zobrist::castling[st.castling & 15];
    if(st.ep != -1) h ^= Zobrist::epFile[st.ep % 8];
    if(st.side == 'b') h ^= Zobrist::side;
    return h;
}

int Board::repetitionCount() const{
    int count = 1; // current pos
    uint64_t cur = positionKey();
    int hm = st.halfmove;
    for(int i=(int)stack.size()-1; i>=0 && hm>0; --i){
        const Undo& u = stack[i];
        if(u.keyBefore == cur) count++;
        hm--; // one ply back
    }
    return count;
}

void Board::setFEN(const std::string& fen){
    std::istringstream ss(fen);
    std::string board_f, side_f, castling_f, ep_f; int half=0, full=1;
    ss >> board_f >> side_f >> castling_f >> ep_f >> half >> full;
    st.board.fill('.');
    int r = 7, f = 0;
    for(char ch : board_f){
        if(ch=='/') { r--; f=0; continue; }
        if(std::isdigit((unsigned char)ch)){ f += ch - '0'; continue; }
        int idx = r*8 + f; st.board[idx] = ch; f++;
    }
    st.side = side_f[0];
    int cr = 0; if(castling_f.find('K')!=std::string::npos) cr|=1; if(castling_f.find('Q')!=std::string::npos) cr|=2; if(castling_f.find('k')!=std::string::npos) cr|=4; if(castling_f.find('q')!=std::string::npos) cr|=8; st.castling = cr;
    st.ep = (ep_f=="-")? -1 : coordToSq(ep_f);
    st.halfmove = half; st.fullmove = full;
    st.wk = -1; st.bk = -1;
    for(int i=0;i<64;i++){ if(st.board[i]=='K') st.wk=i; if(st.board[i]=='k') st.bk=i; }
}

std::string Board::getFEN() const{
    std::string rows;
    for(int r=7;r>=0;--r){
        int empty=0; std::string row;
        for(int f=0;f<8;++f){
            char p = st.board[r*8+f];
            if(p=='.') { empty++; }
            else { if(empty){ row+=std::to_string(empty); empty=0;} row+=p; }
        }
        if(empty) row+=std::to_string(empty);
        rows += row; if(r) rows+='/';
    }
    std::string cr;
    if(st.castling&1) cr+='K'; if(st.castling&2) cr+='Q'; if(st.castling&4) cr+='k'; if(st.castling&8) cr+='q'; if(cr.empty()) cr="-";
    std::string ep = (st.ep==-1? "-" : sqToCoord(st.ep));
    std::ostringstream out; out<<rows<<' '<<st.side<<' '<<cr<<' '<<ep<<' '<<st.halfmove<<' '<<st.fullmove;
    return out.str();
}

bool Board::inKnightBounds(Square from, Square to) const{
    if(!inBounds(to)) return false;
    int ff=from%8, tf=to%8; int fr=from/8, tr=to/8; int df=std::abs(ff-tf), dr=std::abs(fr-tr);
    return (df==1&&dr==2)||(df==2&&dr==1);
}

bool Board::slideOk(Square from, Square to, int d) const{
    if(!inBounds(to)) return false;
    int ff=from%8, tf=to%8; int fr=from/8, tr=to/8;
    if(d== -1 || d==1) return std::abs(tf-ff)==std::abs(to-from);
    if(d== -9 || d== -7 || d==7 || d==9) return std::abs(tf-ff)==std::abs(tr-fr);
    if(d== -8 || d==8) return tf==ff;
    return true;
}

bool Board::kingStepOk(Square from, Square to) const{
    if(!inBounds(to)) return false;
    int df=std::abs(from%8 - to%8), dr=std::abs(from/8 - to/8);
    return std::max(df,dr)==1;
}

bool Board::squareAttacked(Square sq, char bySide) const{
    const auto& b = st.board;
    // pawns
    if(bySide=='w'){
        for(int d : {7,9}){
            int fr = sq - d; if(!inBounds(fr)) continue;
            int sf = sq%8, ff=fr%8; if((d==7 && ff==sf-1) || (d==9 && ff==sf+1)){
                if(b[fr]=='P') return true;
            }
        }
    } else {
        for(int d : {7,9}){
            int fr = sq + d; if(!inBounds(fr)) continue;
            int sf = sq%8, ff=fr%8; if((d==7 && ff==sf+1) || (d==9 && ff==sf-1)){
                if(b[fr]=='p') return true;
            }
        }
    }
    // knights
    for(int d : KNIGHT_DIRS){
        int fr = sq + d; if(!inKnightBounds(sq, fr)) continue; char p = inBounds(fr)? b[fr]:'.';
        if((bySide=='w' && p=='N') || (bySide=='b' && p=='n')) return true;
    }
    // bishops/queens
    for(int d : BISHOP_DIRS){
        int fr = sq + d; while(inBounds(fr) && slideOk(sq, fr, d)){
            char p = b[fr]; if(p!='.'){
                if(bySide=='w' && (p=='B'||p=='Q')) return true;
                if(bySide=='b' && (p=='b'||p=='q')) return true;
                break; }
            fr += d;
        }
    }
    // rooks/queens
    for(int d : ROOK_DIRS){
        int fr = sq + d; while(inBounds(fr) && slideOk(sq, fr, d)){
            char p = b[fr]; if(p!='.'){
                if(bySide=='w' && (p=='R'||p=='Q')) return true;
                if(bySide=='b' && (p=='r'||p=='q')) return true;
                break; }
            fr += d;
        }
    }
    // king
    for(int d : KING_DIRS){ int fr = sq + d; if(kingStepOk(sq, fr)){ char p=inBounds(fr)?b[fr]:'.'; if((bySide=='w'&&p=='K')||(bySide=='b'&&p=='k')) return true; } }
    return false;
}

void Board::genSlides(Square s, char p, const int* dirs, int ndirs, std::vector<Move>& out) const{
    const auto& b = st.board; char side = colorOf(p);
    for(int i=0;i<ndirs;i++){
        int d = dirs[i];
        int to = s + d;
        while(inBounds(to) && slideOk(s, to, d)){
            char q = b[to];
            if(q=='.') out.push_back({s,to,0,0});
            else { if(colorOf(q)!=side) out.push_back({s,to,0,CAPTURE}); break; }
            to += d;
        }
    }
}

void Board::genPawn(Square s, char p, std::vector<Move>& out) const{
    const auto& b = st.board; char side = colorOf(p);
    int dir = (side=='w')? 8 : -8;
    int startRank = (side=='w')? 1:6;
    int to = s + dir;
    if(inBounds(to) && b[to]=='.'){
        int promoRank = (side=='w')?7:0;
        int rankTo = to/8;
        if(rankTo==promoRank){ for(char pr : std::string("QRBN")){ out.push_back({s,to,(side=='w')?pr:char(std::tolower(pr)),PROMOTION}); } }
        else out.push_back({s,to,0,0});
        if(s/8==startRank){ int two = s + 2*dir; if(inBounds(two) && b[two]=='.') out.push_back({s,two,0,DOUBLE_PAWN}); }
    }
    for(int df : {-1,1}){
        int t = s + dir + df; if(!inBounds(t)) continue; if(std::abs((s%8)+df - (t%8))!=0) continue; char q=b[t];
        if(q!='.' && colorOf(q)!=side){ int promoRank=(side=='w')?7:0; int rankTo=t/8; if(rankTo==promoRank){ for(char pr: std::string("QRBN")){ out.push_back({s,t,(side=='w')?pr:char(std::tolower(pr)),PROMOTION|CAPTURE}); } } else out.push_back({s,t,0,CAPTURE}); }
    }
    if(st.ep!=-1){ int ep = st.ep; if( (ep==s+dir-1 && (s%8)>0) || (ep==s+dir+1 && (s%8)<7) ){
        if(std::abs((ep%8)-(s%8))==1 && (ep/8)==(s/8)+((side=='w')?1:-1)) out.push_back({s,ep,0,EN_PASSANT|CAPTURE});
    }}
}

void Board::genKing(Square s, char p, std::vector<Move>& out) const{
    const auto& b = st.board; char side=colorOf(p);
    for(int d : KING_DIRS){ int to=s+d; if(!kingStepOk(s,to)) continue; char q = inBounds(to)?b[to]:'.'; if(q=='.' || colorOf(q)!=side) out.push_back({s,to,0, (q!='.')?CAPTURE:0}); }
    genCastles(s, p, out);
}

void Board::genCastles(Square ksq, char k, std::vector<Move>& out) const{
    const auto& b = st.board; char side = (k=='K')?'w':'b';
    if(side=='w'){
        if((st.castling&1) && b[5]=='.' && b[6]=='.'){
            if(!squareAttacked(4,'b') && !squareAttacked(5,'b') && !squareAttacked(6,'b')) out.push_back({4,6,0,CASTLE});
        }
        if((st.castling&2) && b[1]=='.' && b[2]=='.' && b[3]=='.'){
            if(!squareAttacked(4,'b') && !squareAttacked(3,'b') && !squareAttacked(2,'b')) out.push_back({4,2,0,CASTLE});
        }
    } else {
        if((st.castling&4) && b[61]=='.' && b[62]=='.'){
            if(!squareAttacked(60,'w') && !squareAttacked(61,'w') && !squareAttacked(62,'w')) out.push_back({60,62,0,CASTLE});
        }
        if((st.castling&8) && b[57]=='.' && b[58]=='.' && b[59]=='.'){
            if(!squareAttacked(60,'w') && !squareAttacked(59,'w') && !squareAttacked(58,'w')) out.push_back({60,58,0,CASTLE});
        }
    }
}

std::vector<Move> Board::generateLegalMoves(){
    std::vector<Move> moves, legal;
    const auto& b = st.board; char side = st.side;
    int pinnedDir[64]; computePins(side, pinnedDir);
    for(int s=0;s<64;++s){ char p=b[s]; if(p=='.' || colorOf(p)!=side) continue;
        switch(std::toupper(p)){
            case 'P': genPawn(s,p,moves); break;
            case 'N': for(int d:KNIGHT_DIRS){ int to=s+d; if(!inKnightBounds(s,to)) continue; char q=inBounds(to)?b[to]:'.'; if(q=='.' || colorOf(q)!=colorOf(p)) moves.push_back({s,to,0, (q!='.')?CAPTURE:0}); } break;
            case 'B': genSlides(s,p,BISHOP_DIRS,4,moves); break;
            case 'R': genSlides(s,p,ROOK_DIRS,4,moves); break;
            case 'Q': genSlides(s,p,QUEEN_DIRS,8,moves); break;
            case 'K': genKing(s,p,moves); break;
        }
    }
    auto alignedWith = [&](int from,int to,int d){ if(d==0) return true; int delta = to-from; if(delta==0) return true; if(delta % d != 0) return false; return slideOk(from,to,d); };
    for(const auto& m : moves){
        char pc = b[m.from]; if(std::toupper(pc)!='K' && pinnedDir[m.from]!=0){ if(!alignedWith(m.from, m.to, pinnedDir[m.from])) continue; }
        if(makeMove(m)){
            // makeMove already ensures own king is not left in check
            legal.push_back(m);
            unmakeMove();
        }
    }
    return legal;
}

bool Board::makeMove(const Move& m){
    auto& b = st.board; char side = st.side; char fromP = b[m.from]; char toP = b[m.to]; char placed = fromP; char captured = toP;
    Undo u; u.m = m; u.captured = captured; u.castling=st.castling; u.ep=st.ep; u.halfmove=st.halfmove; u.fullmove=st.fullmove; u.wk=st.wk; u.bk=st.bk; u.movedFrom=fromP; u.movedTo=toP; u.keyBefore = positionKey(); stack.push_back(u);

    if(std::toupper(fromP)=='P' || captured!='.') st.halfmove=0; else st.halfmove++;
    st.ep = -1;

    b[m.from] = '.';
    if(m.flags & PROMOTION) placed = m.promo;

    if(m.flags & EN_PASSANT){ int capSq = (side=='w')? m.to-8 : m.to+8; captured = b[capSq]; b[capSq]='.'; }

    b[m.to] = placed;

    if(m.flags & CASTLE){
        if(placed=='K'){
            if(m.to==6){ b[7]='.'; b[5]='R'; } else { b[0]='.'; b[3]='R'; }
            st.wk = m.to;
        } else if(placed=='k'){
            if(m.to==62){ b[63]='.'; b[61]='r'; } else { b[56]='.'; b[59]='r'; }
            st.bk = m.to;
        }
    } else {
        if(placed=='K') st.wk=m.to; else if(placed=='k') st.bk=m.to;
    }

    if(m.flags & DOUBLE_PAWN){ st.ep = (side=='w')? m.from+8 : m.from-8; }

    if(fromP=='K') st.castling &= ~(1|2);
    if(fromP=='R'){ if(m.from==0) st.castling &= ~2; if(m.from==7) st.castling &= ~1; }
    if(captured=='R'){ if(m.to==0) st.castling &= ~2; if(m.to==7) st.castling &= ~1; }
    if(fromP=='k') st.castling &= ~(4|8);
    if(fromP=='r'){ if(m.from==56) st.castling &= ~8; if(m.from==63) st.castling &= ~4; }
    if(captured=='r'){ if(m.to==56) st.castling &= ~8; if(m.to==63) st.castling &= ~4; }

    if(side=='b') st.fullmove++;
    st.side = (side=='w')? 'b':'w';

    // Illegal if own king in check
    char own = (st.side=='w')?'b':'w'; int ksq = (own=='w')? st.wk : st.bk; if(squareAttacked(ksq, st.side)){ unmakeMove(); return false; }
    return true;
}

void Board::unmakeMove(){
    assert(!stack.empty());
    Undo u = stack.back(); stack.pop_back();
    auto& b = st.board;
    st.castling=u.castling; st.ep=u.ep; st.halfmove=u.halfmove; st.fullmove=u.fullmove; st.wk=u.wk; st.bk=u.bk; st.side = (st.side=='w')? 'b':'w';

    char moved = b[u.m.to];
    if(u.m.flags & CASTLE){
        if(moved=='K'){ if(u.m.to==6){ b[5]='.'; b[7]='R'; } else { b[3]='.'; b[0]='R'; } }
        else if(moved=='k'){ if(u.m.to==62){ b[61]='.'; b[63]='r'; } else { b[59]='.'; b[56]='r'; } }
    }
    if(u.m.flags & EN_PASSANT){ if(st.side=='w'){ int cap=u.m.to-8; b[cap]='p'; } else { int cap=u.m.to+8; b[cap]='P'; } }

    b[u.m.from] = u.movedFrom; if(u.m.flags & PROMOTION) b[u.m.from] = (std::isupper((unsigned char)u.movedFrom)? 'P':'p');
    b[u.m.to] = u.captured;
}

} // namespace eng
