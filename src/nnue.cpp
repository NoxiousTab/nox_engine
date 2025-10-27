#include "nnue.h"
#include "board.h"
#include <atomic>
#include <string>
#include <fstream>
#include <cstdint>
#include <vector>
#include <cstring>
#include <cmath>

namespace eng {

static std::atomic<bool> g_nnueEnabled{false};
static std::atomic<bool> g_nnueReady{false};
static std::string g_nnuePath;
static std::string g_nnueDesc;
static uint32_t g_nnueVersion{0};
static uint32_t g_nnueHash{0};

static inline uint32_t read_le_u32(std::istream& s){ uint32_t v=0; s.read(reinterpret_cast<char*>(&v), sizeof(v)); return v; }

// NOXNET weights
static uint32_t g_inDim=0, g_h1=0, g_h2=0, g_outDim=0;
static std::vector<float> g_w1, g_b1, g_w2, g_b2, g_w3, g_b3;

static bool read_vec(std::istream& f, std::vector<float>& v, size_t count){
    v.resize(count);
    return (bool)f.read(reinterpret_cast<char*>(v.data()), sizeof(float)*count);
}

static inline int orientSq(int sq, char side){
    if(side=='w') return sq;
    // flip both rank and file for a simple side-relative mapping
    return 63 - sq;
}

static inline int pieceIndex(char p){
    switch(p){
        case 'P': return 0; case 'N': return 1; case 'B': return 2; case 'R': return 3; case 'Q': return 4; case 'K': return 5;
        case 'p': return 6; case 'n': return 7; case 'b': return 8; case 'r': return 9; case 'q': return 10; case 'k': return 11;
        default: return -1;
    }
}

static void build_features(const Board& b, std::vector<float>& x){
    // Expected layout:
    // [0..767): 12*64 piece-square one-hot, side-relative
    // [768]: side to move (1 if white to move else 0)
    // [769..772]: castling KQkq
    // [773..780]: ep file one-hot (8), 0 if none
    // [781]: phase scalar in [0,1]
    const uint32_t expected = 782;
    x.assign(g_inDim ? g_inDim : expected, 0.0f);
    const auto& brd = b.st.board;
    char side = b.st.side;
    // pieces
    for(int s=0;s<64;++s){ char p=brd[s]; if(p=='.') continue; int pi = pieceIndex(p); if(pi<0) continue; int os = orientSq(s, side); int idx = pi*64 + os; if(idx>=0 && idx<768 && idx<(int)x.size()) x[idx]=1.0f; }
    // side to move
    if(768 < (int)x.size()) x[768] = (side=='w')? 1.0f : 0.0f;
    // castling flags
    if(772 < (int)x.size()){
        int c = b.st.castling; if(c & 1) x[769]=1.0f; if(c & 2) x[770]=1.0f; if(c & 4) x[771]=1.0f; if(c & 8) x[772]=1.0f;
    }
    // en-passant file
    if(780 < (int)x.size()){
        int ep = b.st.ep; if(ep>=0){ int file = ep % 8; x[773+file] = 1.0f; }
    }
    // phase scalar: based on non-king material
    if(781 < (int)x.size()){
        auto absVal = [](char p){ switch(p){ case 'P':case 'p': return 1; case 'N':case 'n': return 3; case 'B':case 'b': return 3; case 'R':case 'r': return 5; case 'Q':case 'q': return 9; default: return 0; } }; // coarse
        int total=0; for(int i=0;i<64;i++){ total += absVal(brd[i]); }
        float phase = std::fmin(1.0f, total / 78.0f); // 2*(9+2*5+2*3+2*3+8*1)=78 mid-ish
        x[781] = phase;
    }
}

bool NNUE::load(const std::string& path){
    g_nnuePath = path;
    g_nnueReady = false;
    std::ifstream f(path, std::ios::binary);
    if(!f) return false;
    // Magic
    char magic[8]{}; f.read(magic, 8); if(!f) return false;
    if(std::strncmp(magic, "NOXNET1", 7)!=0) return false;
    g_nnueVersion = read_le_u32(f);
    g_inDim = read_le_u32(f);
    g_h1    = read_le_u32(f);
    g_h2    = read_le_u32(f);
    g_outDim= read_le_u32(f);
    if(!f || g_outDim!=1 || g_inDim==0 || g_h1==0 || g_h2==0) return false;
    size_t w1c = size_t(g_inDim)*g_h1, b1c=g_h1;
    size_t w2c = size_t(g_h1)*g_h2,   b2c=g_h2;
    size_t w3c = size_t(g_h2)*g_outDim, b3c=g_outDim;
    if(!read_vec(f, g_w1, w1c)) return false;
    if(!read_vec(f, g_b1, b1c)) return false;
    if(!read_vec(f, g_w2, w2c)) return false;
    if(!read_vec(f, g_b2, b2c)) return false;
    if(!read_vec(f, g_w3, w3c)) return false;
    if(!read_vec(f, g_b3, b3c)) return false;
    g_nnueReady = true;
    return true;
}

bool NNUE::isReady(){ return g_nnueReady.load(); }
void NNUE::setEnabled(bool on){ g_nnueEnabled = on; }
bool NNUE::isEnabled(){ return g_nnueEnabled.load(); }

int NNUE::evaluate(const Board& b){
    if(!g_nnueEnabled.load() || !g_nnueReady.load()) return 0;
    // Build features
    std::vector<float> x; build_features(b, x);
    // Forward: y1 = relu(W1^T x + b1)
    std::vector<float> y1(g_h1, 0.0f), y2(g_h2, 0.0f);
    // W1 is (in x h1) laid row-major as in reading order: we stored weights as an array size in*h1
    for(uint32_t j=0;j<g_h1;++j){
        double s = g_b1[j];
        const float* wcol = &g_w1[size_t(j)];
        for(uint32_t i=0;i<g_inDim;++i){ s += wcol[size_t(i)*g_h1] * x[i]; }
        y1[j] = s>0.0 ? float(s) : 0.0f;
    }
    // y2 = relu(W2^T y1 + b2)
    for(uint32_t j=0;j<g_h2;++j){
        double s = g_b2[j];
        const float* wcol = &g_w2[size_t(j)];
        for(uint32_t i=0;i<g_h1;++i){ s += wcol[size_t(i)*g_h2] * y1[i]; }
        y2[j] = s>0.0 ? float(s) : 0.0f;
    }
    // out = W3^T y2 + b3
    double out = g_b3[0];
    for(uint32_t i=0;i<g_h2;++i){ out += g_w3[i] * y2[i]; }
    // return centipawns
    if(out > 30000) out = 30000; if(out < -30000) out = -30000;
    return int(std::lround(out));
}

} // namespace eng
