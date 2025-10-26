#pragma once
#include <string>
#include "board.h"
#include "search.h"

namespace eng {

class UCI {
public:
    void loop();

private:
    Board board;
    Searcher searcher;
    bool debug{false};
    int skill{10};
    int threads{1};
    bool useBook{true};
    bool useNNUE{false};
    std::string evalFile;

    void cmdPosition(const std::string& line);
    void cmdGo(const std::string& line);
    void cmdSetOption(const std::string& line);
    Move parseUciMove(const std::string& s);
    std::string moveToUci(const Move& m) const;
    bool tryBookMove(Move& out);
};

} // namespace eng
