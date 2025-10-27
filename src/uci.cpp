#include "uci.h"
#include "nnue.h"
#include "eval.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace eng {

static std::string trim(const std::string& s){
    size_t a = s.find_first_not_of(" \t\r\n"); if(a==std::string::npos) return ""; size_t b = s.find_last_not_of(" \t\r\n"); return s.substr(a, b-a+1);
}

static uint64_t perftRec(Board& b, int depth){
    if(depth==0) return 1ULL;
    uint64_t nodes=0; auto moves=b.generateLegalMoves();
    for(const auto& m: moves){ if(!b.makeMove(m)) continue; nodes += perftRec(b, depth-1); b.unmakeMove(); }
    return nodes;
}

bool UCI::tryBookMove(Move& out){
    // Tiny built-in book: startpos first move only
    static int rot = 0; // simple round-robin
    std::string fen = board.getFEN();
    // Startpos with white to move and no moves played (only then, avoid unthinking replies)
    if(fen.rfind("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w", 0) == 0){
        std::vector<std::string> cands = {"e2e4","d2d4","c2c4","g1f3"};
        std::string s = cands[rot++ % cands.size()];
        out = parseUciMove(s);
        if(out.from||out.to) return true;
    }
    return false;
}

void UCI::loop(){
    // one-time init
    searcher.tt.resizeMB(16);
    board.setStartPos();

    std::string line;
    while (std::getline(std::cin, line)){
        line = trim(line);
        if(line.empty()) continue;
        if(debug) std::cerr << "[debug] recv: " << line << std::endl;
        if(line == "uci"){
            std::cout << "id name nox_engine" << std::endl;
            std::cout << "id author Ahmed Tabish" << std::endl;
            std::cout << "option name Skill Level type spin default 10 min 1 max 20" << std::endl;
            std::cout << "option name Debug type check default false" << std::endl;
            std::cout << "option name Hash type spin default 16 min 1 max 4096" << std::endl;
            std::cout << "option name Contempt type spin default 0 min -200 max 200" << std::endl;
            std::cout << "option name Threads type spin default 1 min 1 max 64" << std::endl;
            std::cout << "option name UseBook type check default true" << std::endl;
            std::cout << "option name Use NNUE type check default false" << std::endl;
            std::cout << "option name EvalFile type string default" << std::endl;
            std::cout << "uciok" << std::endl;
            std::cout.flush();
        } else if(line == "isready"){
            std::cout << "readyok" << std::endl;
            std::cout.flush();
        } else if(line.rfind("setoption",0)==0){
            cmdSetOption(line);
        } else if(line.rfind("perft",0)==0){
            std::istringstream ss(line); std::string w; ss>>w; int d=1; ss>>d; if(d<0) d=0; Board tmp=board; uint64_t n=perftRec(tmp,d); std::cout<<n<<std::endl; std::cout.flush();
        } else if(line.rfind("evalfen ",0)==0){
            std::string fen = line.substr(8);
            Board tmp; tmp.setFEN(fen);
            int score = 0;
            if (NNUE::isEnabled() && NNUE::isReady()) score = NNUE::evaluate(tmp);
            else score = Eval::evaluate(tmp);
            std::cout << score << std::endl; std::cout.flush();
        } else if(line == "ucinewgame"){
            board.setStartPos();
        } else if(line.rfind("position",0)==0){
            cmdPosition(line);
        } else if(line.rfind("go",0)==0){
            cmdGo(line);
        } else if(line == "stop"){
            searcher.stop = true;
        } else if(line == "quit"){
            break;
        } else if(line.rfind("debug",0)==0){
            std::istringstream ss(line); std::string w, v; ss>>w>>v; if(!v.empty()) debug = (v=="on");
        }
    }
}

void UCI::cmdSetOption(const std::string& line){
    // setoption name <name> value <val>
    std::istringstream ss(line);
    std::string word; ss >> word; // setoption
    std::string name, value; bool inName=false, inValue=false;
    while(ss >> word){
        if(word == "name") { inName=true; inValue=false; continue; }
        if(word == "value") { inValue=true; inName=false; continue; }
        if(inName){ if(!name.empty()) name += ' '; name += word; }
        else if(inValue){ if(!value.empty()) value += ' '; value += word; }
    }
    std::string lname = name; std::transform(lname.begin(), lname.end(), lname.begin(), ::tolower);
    if(lname == "skill level"){
        try{ int v = std::stoi(value); if(v<1) v=1; if(v>20) v=20; skill=v; searcher.maxDepth = std::max(2, std::min(20, v)); } catch(...){}
    } else if(lname == "debug"){
        std::string lv = value; std::transform(lv.begin(), lv.end(), lv.begin(), ::tolower); debug = (lv=="true"||lv=="on"||lv=="1");
    } else if(lname == "hash"){
        try{ int mb = std::stoi(value); if(mb<1) mb=1; if(mb>4096) mb=4096; searcher.tt.resizeMB((size_t)mb); } catch(...){}
    } else if(lname == "contempt"){
        try{ int c = std::stoi(value); if(c<-200) c=-200; if(c>200) c=200; searcher.contempt = c; } catch(...){}
    } else if(lname == "threads"){
        try{ int t = std::stoi(value); if(t<1) t=1; if(t>64) t=64; threads=t; searcher.threads=t; } catch(...){}
    } else if(lname == "usebook"){
        std::string lv = value; std::transform(lv.begin(), lv.end(), lv.begin(), ::tolower); useBook = (lv=="true"||lv=="on"||lv=="1");
    } else if(lname == "use nnue"){
        std::string lv = value; std::transform(lv.begin(), lv.end(), lv.begin(), ::tolower); useNNUE = (lv=="true"||lv=="on"||lv=="1");
        NNUE::setEnabled(useNNUE);
    } else if(lname == "evalfile"){
        evalFile = value;
        if(!evalFile.empty()) { NNUE::load(evalFile); }
    }
    if(debug) std::cerr << "[debug] setoption name="<<name<<" value="<<value<<" depth="<<searcher.maxDepth<< std::endl;
}

void UCI::cmdPosition(const std::string& line){
    // position [startpos|fen <6 tokens>] [moves ...]
    std::istringstream ss(line);
    std::string word; ss >> word; // position
    ss >> word;
    if(word == "startpos"){
        board.setStartPos();
        ss >> word; // maybe moves
    } else if(word == "fen"){
        std::string f1,f2,f3,f4,f5,f6; ss >> f1 >> f2 >> f3 >> f4 >> f5 >> f6; std::string fen = f1+" "+f2+" "+f3+" "+f4+" "+f5+" "+f6; board.setFEN(fen); ss >> word; // maybe moves
    }
    if(word == "moves"){
        std::string mv;
        while(ss >> mv){ Move m = parseUciMove(mv); if(m.from||m.to) board.makeMove(m); }
    }
    if(debug) std::cerr << "[debug] position -> "<< board.getFEN() << std::endl;
}

Move UCI::parseUciMove(const std::string& s){
    if(s.size() < 4) return Move{};
    Square from = coordToSq(s.substr(0,2));
    Square to   = coordToSq(s.substr(2,2));
    char promo = 0; if(s.size()>=5){ char c = std::tolower(s[4]); if(c=='q'||c=='r'||c=='b'||c=='n'){ promo = (board.st.side=='w')? std::toupper(c) : c; } }
    auto moves = board.generateLegalMoves();
    for(const auto& m: moves){ if(m.from==from && m.to==to){ if((m.flags & PROMOTION)){ if(promo && m.promo==promo) return m; else continue; } return m; } }
    return Move{};
}

std::string UCI::moveToUci(const Move& m) const{
    std::string s = sqToCoord(m.from) + sqToCoord(m.to);
    if((m.flags & PROMOTION) && m.promo) s += (char)std::tolower(m.promo);
    return s;
}

void UCI::cmdGo(const std::string& line){
    // go wtime 300000 btime 300000 winc 2000 binc 2000 movestogo 40 depth 10 movetime 1000
    std::istringstream ss(line);
    std::string word; ss >> word; // go
    int wtime=-1,btime=-1,winc=0,binc=0,movestogo=30,depth=0,movetime=-1;
    while(ss>>word){
        if(word=="wtime") ss>>wtime; else if(word=="btime") ss>>btime; else if(word=="winc") ss>>winc; else if(word=="binc") ss>>binc; else if(word=="movestogo") ss>>movestogo; else if(word=="depth") ss>>depth; else if(word=="movetime") ss>>movetime; }
    int useDepth = depth? depth : searcher.maxDepth;
    int timeMs = 1000;
    if(movetime>0) timeMs = movetime; else {
        if(board.st.side=='w' && wtime>=0) timeMs = std::max(10, wtime/ (movestogo>0? movestogo:30) + winc/2);
        else if(board.st.side=='b' && btime>=0) timeMs = std::max(10, btime/ (movestogo>0? movestogo:30) + binc/2);
        else timeMs = 1000;
    }
    if(debug) std::cerr << "[debug] go timeMs="<<timeMs<<" depth="<<useDepth<< std::endl;
    // Try to load NNUE at go time if enabled and not yet ready
    if(useNNUE && !evalFile.empty() && !NNUE::isReady()){
        NNUE::load(evalFile);
    }
    int prevDepth = searcher.maxDepth; searcher.maxDepth = useDepth; searcher.threads = threads;
    // Try book move if enabled
    if(useBook){ Move bm; if(tryBookMove(bm)){ std::cout << "bestmove " << moveToUci(bm) << std::endl; std::cout.flush(); return; } }
    SearchResult res = searcher.search(board, timeMs);
    searcher.maxDepth = prevDepth;
    if(res.best.from==0 && res.best.to==0){ std::cout << "bestmove 0000" << std::endl; }
    else { std::cout << "bestmove " << moveToUci(res.best) << std::endl; }
    std::cout.flush();
}

} // namespace eng
