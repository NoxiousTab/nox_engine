#pragma once
#include <string>
#include <thread>
#include "board.h"
#include "search.h"

namespace eng {

class UCI {
public:
    void loop();
    ~UCI(); // ensures any in-flight search thread is stopped and joined before destruction

private:
    Board board;
    Searcher searcher;
    bool debug{false};
    int skill{10};
    int threads{1};
    bool useBook{true};
    bool useNNUE{false};
    std::string evalFile;
    std::thread searchThread; // runs Searcher::search() in the background so the
                               // main loop stays free to read "stop"/"quit" while
                               // a search is in progress

    void cmdPosition(const std::string& line);
    void cmdGo(const std::string& line);
    void cmdSetOption(const std::string& line);
    Move parseUciMove(const std::string& s);
    std::string moveToUci(const Move& m) const;
    bool tryBookMove(Move& out);
    void stopAndJoinSearch(); // if a search is in flight, signal it to stop and
                               // wait for it to finish before touching any shared
                               // engine state (board, searcher config, TT, etc.)
};

}