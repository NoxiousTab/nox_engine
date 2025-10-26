#include <iostream>
#include "uci.h"
#include "zobrist.h"

int main(){
    eng::Zobrist::init();
    eng::UCI uci;
    uci.loop();
    return 0;
}
