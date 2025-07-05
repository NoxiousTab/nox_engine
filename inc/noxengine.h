#pragma once
#define ui64 uint64_t
#include "generation.h"
#include "nnue.h"
#include "report.h"
#include "position.h"
#include "search.h"
#include "settings.h"
#include <fstream>
#include <iomanip>
#include <random>
#include <thread>
#include <tuple>

void generateMagicTables();

enum class modes {
    normal,
    bench,
    genNormal,
    genFRC
};

class noxengine {
    public:
    noxengine(int argc, char *argv[]);
    void init();
    void debugHeader() const;
    void drawBoard(const position &pos, const ui64 contrast = 0) const;
    void handleBench();
    void handleHelp() const;
    void handleCompile() const;
    search searchThreads;
    modes mode = modes::normal;

#ifdef __cpp_lib_format
    const bool prettyHelp = true;
#else
    const bool prettyHelp = false;
#endif

};