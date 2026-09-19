#include <iostream>
#include "uci.h"
#include "zobrist.h"
#include "bitboard.h"

int main(){
    eng::Zobrist::init();
    eng::initBitboardTables();
    eng::UCI uci;
    uci.loop();
    return 0;
}