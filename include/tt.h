#pragma once
#include <cstdint>
#include <vector>
#include <cstring>
#include <mutex>
#include "types.h"

namespace eng {

enum class Bound : uint8_t { Exact=0, Lower=1, Upper=2 };

struct TTEntry {
    uint64_t key{0};
    int32_t score{0}; // widened from int16_t: mate scores reach +/-100000,
                       // which overflowed int16_t (max +/-32767) and wrapped
                       // into garbage values on store.
    int8_t depth{0};
    uint8_t bound{0};
    Move best{};
};

class TT {
public:
    TT() = default;
    void resizeMB(size_t mb){
        std::lock_guard<std::mutex> lock(mtx);
        size_t bytes = mb * 1024ull * 1024ull;
        size_t n = bytes / sizeof(TTEntry);
        if(n < 1024) n = 1024;
        table.assign(n, {});
        mask = n - 1;
        // if not power of two, we'll handle modulo
        mod = ( (n & (n-1))==0 ) ? 0 : n;
    }
    bool probe(uint64_t key, TTEntry& out) const{
        if(table.empty()) return false;
        std::lock_guard<std::mutex> lock(mtx);
        const TTEntry& e = at(key);
        if(e.key == key){ out = e; return true; }
        return false;
    }
    void clear(){
        // Wipes all entries in place without touching the configured size
        // (unlike resizeMB, which also reallocates). Needed on "ucinewgame":
        // otherwise stale entries from a completely different, earlier
        // position/game keep being probed and trusted for the new one.
        std::lock_guard<std::mutex> lock(mtx);
        table.assign(table.size(), TTEntry{});
    }
    void store(uint64_t key, int depth, int score, Bound bnd, const Move& best){
        if(table.empty()) return;
        std::lock_guard<std::mutex> lock(mtx);
        TTEntry e; e.key=key; e.depth=(int8_t)depth; e.score=(int32_t)score; e.bound=(uint8_t)bnd; e.best=best;
        TTEntry& dst = ref(key);
        // replace if deeper or empty
        if(dst.key==0 || depth >= dst.depth) dst = e;
    }
private:
    std::vector<TTEntry> table;
    size_t mask{0};
    size_t mod{0};
    mutable std::mutex mtx;
    const TTEntry& at(uint64_t key) const{
        if(mod) return table[key % mod];
        return table[key & mask];
    }
    TTEntry& ref(uint64_t key){
        if(mod) return table[key % mod];
        return table[key & mask];
    }
};

}