#include "nnue.h"
#include "board.h"
#include <atomic>
#include <string>
#include <fstream>
#include <cstdint>

namespace eng {

static std::atomic<bool> g_nnueEnabled{false};
static std::atomic<bool> g_nnueReady{false};
static std::string g_nnuePath;
static std::string g_nnueDesc;
static uint32_t g_nnueVersion{0};
static uint32_t g_nnueHash{0};

static inline uint32_t read_le_u32(std::istream& s){ uint32_t v=0; s.read(reinterpret_cast<char*>(&v), sizeof(v)); return v; }

bool NNUE::load(const std::string& path){
    g_nnuePath = path;
    g_nnueReady = false;
    std::ifstream f(path, std::ios::binary);
    if(!f) return false;
    // Read header: Version (u32), Hash (u32), Desc size (u32), Desc bytes
    g_nnueVersion = read_le_u32(f);
    g_nnueHash = read_le_u32(f);
    uint32_t dsz = read_le_u32(f);
    if(!f) return false;
    g_nnueDesc.resize(dsz);
    if(dsz) f.read(&g_nnueDesc[0], dsz);
    if(!f) return false;
    // We do not parse parameters yet; mark ready so NNUE path is exercised (provisional)
    g_nnueReady = true;
    return true;
}

bool NNUE::isReady(){ return g_nnueReady.load(); }
void NNUE::setEnabled(bool on){ g_nnueEnabled = on; }
bool NNUE::isEnabled(){ return g_nnueEnabled.load(); }

static inline int matVal(char p){
    switch(p){
        case 'P': return 100; case 'N': return 320; case 'B': return 330; case 'R': return 500; case 'Q': return 900; case 'K': return 0;
        case 'p': return -100; case 'n': return -320; case 'b': return -330; case 'r': return -500; case 'q': return -900; case 'k': return 0;
        default: return 0;
    }
}

int NNUE::evaluate(const Board& b){
    // Provisional evaluator while full NNUE forward is implemented:
    // fast material-only with slight tempo to avoid zero scores.
    int s=0; const auto& brd=b.st.board;
    for(int i=0;i<64;i++){ char p=brd[i]; if(p!='.') s+=matVal(p); }
    if(b.st.side=='w') s+=8; else s-=8;
    return s;
}

} // namespace eng
