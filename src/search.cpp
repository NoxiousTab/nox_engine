#include "../inc/search.h"

search::search() {
    constexpr double multiplier = 0.40;
    constexpr double base = 0.76;

    for(int i = 1; i < 32; i++) {
        for(int j = 1; j < 32; j++) {
            lmrTable[i][j] = static_cast<int>(multiplier * std::log(i) * std::log(j) + base);
        }
    }
    initThreads(1);
}

void threadContext::resetStats() {
    rootD = 0;
    selD = 0;
    nodes = 0;
}

void search::resetState(const bool clearT) {
    for(threadContext &t : threads) {
        t.hist.clrAll();
    }
    if(clearT) {
        table.clr(settings::threads);
    }
}


void search::initThreads(const int count) {
    assert(threads.size() == 0);
    loadedThreads.store(0);
    for(int i = 0; i < count; i++) {
        threadContext &t = threads.emplace_back();
        t.id = i;
        t.Thread = std::thread([&] {loop(t); });
    }
    while(loadedThreads.load() < static_cast<int>(threads.size())) {};
}

void search::stopThreads() {
    stopSearch();
    for(threadContext &t : threads) {
        std::unique_lock<std::mutex> lock(t.Mut);
        t.act = thread_action::quit;
        lock.unlock();
        t.condVar.notify_one();
    }
    for(threadContext &t : threads) {
        t.Thread.join();
    }
    threads.clear();
}


int main() {
    cout<<"Works\n";
    return 0;
}