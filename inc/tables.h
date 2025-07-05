#pragma once
#define ui32 uint32_t
#define ui64 uint64_t
#define ui16 uint16_t
#define ui8 uint8_t
#define i16 int16_t
#include "move.h"
#include "utils.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <memory>
#include <thread>

namespace score_type {
    constexpr int invalid = 0;
    constexpr int exact = 1;
    constexpr int upperB = 2;
    constexpr int lowerB = 3;
};


struct tableEntry {
    ui32 hash;
    i16 score;
    i16 raw_eval;
    ui16 generation;
    ui8 depth;
    ui8 scoretype;
    ui16 packed;
    bool variation;

    inline bool canCut(const int searchDepth, const int alpha, const int beta) {
        if(searchDepth > depth) {
            return false;
        }
        if(scoretype == score_type::exact) {
            return true;
        }
        
        if(scoretype == score_type::upperB && score <= alpha) {
            return true;
        }

        if(scoretype == score_type::lowerB && score >= beta) {
            return true;
        }

        return false;

    }

};

struct alignas(64) cluster {
    std::array<tableEntry, 4> entries;
};

static_assert(sizeof(tableEntry) == 16);
static_assert(sizeof(cluster) == 64);

class tables {
    public:
    tables();
    ~tables();
    void store(const ui64 hash, const int depth, const i16 score, const int scoretype, const i16 raw_eval, const Move &mov, const int level, const bool variation);
    bool probe(const ui64 hash, tableEntry &entry, const int level) const;
    void preFetch(const ui64 hash) const;
    void increaseAge();
    void setSize(const int mB, const int threadCnt);
    void clr(const int threadCnt);
    int getFullHash() const;

    private:
    cluster *table = nullptr;
    ui64 table_size = 0;
    ui16 currGen;

    void allocateTable(const ui64 clusterCnt);
    void freeTable();

    inline ui64 getClusterInd(const ui64 hash) const {
        assert((table_size & (table_size - 1)) == 0);
        return hash & (table_size - 1);
    }

    inline ui32 getStoredHash(const ui64 hash) const {
        return static_cast<ui32>((hash & 0xFFFFFFFF00000000) >> 32);
    }

    inline int recQuality(const int generation, const int depth) const {
        return 2*generation + depth;
    }
};