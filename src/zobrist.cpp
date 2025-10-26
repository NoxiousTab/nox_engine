#include "zobrist.h"
#include <random>

namespace eng {

std::array<std::array<uint64_t,64>,12> Zobrist::piece{};
std::array<uint64_t,16> Zobrist::castling{};
std::array<uint64_t,8> Zobrist::epFile{};
uint64_t Zobrist::side = 0;

static uint64_t splitmix64(uint64_t& x){
    uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

void Zobrist::init(){
    uint64_t seed = 0x123456789abcdefULL; // fixed seed for reproducibility
    for(int p=0;p<12;++p){
        for(int sq=0;sq<64;++sq){ piece[p][sq] = splitmix64(seed); }
    }
    for(int i=0;i<16;++i) castling[i] = splitmix64(seed);
    for(int f=0; f<8; ++f) epFile[f] = splitmix64(seed);
    side = splitmix64(seed);
}

} // namespace eng
