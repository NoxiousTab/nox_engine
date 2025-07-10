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


void search::setThreads(const int cnt) {
    stopThreads();
    initThreads(cnt);
}

results search::searchSingleThread(const position &pos, const searchParameters &pars) {
    
    startSearchTime = Clk::now();
    aborting.store(false);
    table.increaseAge();
    threadContext &t = threads.front();
    t.singleThread = true;
    t.currPos = pos;
    t.res = {};
    t.resetStats();
    constraints = calcParams(pars, pos.turn());

    searchMoves(t);
    t.singleThread = false;
    return t.res;

}

void search::startSearch(position &pos, const searchParameters pars) {
    startSearchTime = Clk::now();
    table.increaseAge();

    moveList legalMoves{};
    pos.moveGeneration(legalMoves, moveGen::All, Legality::Legal);

    if(legalMoves.size() == 0) {
        cout <<"No legal moves!\n";
        debugBestMove(nullMove);
        return;
    }

    constraints = calcParams(pars, pos.turn());

    if(legalMoves.size() == 1 && (pars.wTime != 0 || pars.bTime != 0)) {
        cout<<"Only one legal move, reducing search time\n";
        constraints.minSearchTime = std::min(constraints.minSearchTime, 200);
        constraints.maxSearchTime = std::min(constraints.maxSearchTime, 2000);
    }

    aborting.store(false);
    activeThreads.store(threads.size());
    for(threadContext &t : threads) {
        t.currPos = pos;
        t.res = {};
        t.resetStats();
    }

    for(threadContext &t : threads) {
        std::unique_lock<std::mutex> lock(t.Mut);
        t.act = thread_action::search;
        lock.unlock();
        t.condVar.notify_one();
    }
}

void search::stopSearch() {
    aborting.store(true);
    waitUntilReady();
}

void search::loop(threadContext &t) {
    loadedThreads.fetch_add(1);

    while(1) {
        std::unique_lock<std::mutex> lock(t.Mut);
        t.condVar.wait(lock, [&]{
            return t.act != thread_action::sleep;
        });

        if(t.act == thread_action::quit) {
            break;
        }else {
            searchMoves(t);
            if(t.isThreadMain()) {
                debugBestMove(t.res.bestMove());
            }
        }

        t.act = thread_action::sleep;
        activeThreads.fetch_sub(1);
        t.condVar.notify_one();
    }

    std::unique_lock<std::mutex> lock(t.Mut);
    t.quited = true;
    lock.unlock();
    t.condVar.notify_all();
}

void search::waitUntilReady() {
    for(threadContext &t : threads) {
        std::unique_lock<std::mutex> lock(t.Mut);
        t.condVar.wait(lock, [&] {
            return t.act == thread_action::sleep;
        });
    }
}

searchConstraints search::calcParams(const searchParameters params, const bool turn) const {
    searchConstraints cons;
    
    if(params.nodes != 0) {
        cons.maxNodes = params.nodes;
    }
    if(params.softnodes != 0) {
        cons.SoftNodes = params.softnodes;
    }
    if(params.moveTime != 0) {
        cons.minSearchTime = params.moveTime;
        cons.maxSearchTime = params.moveTime;
    }

    if(cons.maxDepth != -1 || cons.maxNodes != -1) {
        return cons;
    }

    

    return cons;

}
