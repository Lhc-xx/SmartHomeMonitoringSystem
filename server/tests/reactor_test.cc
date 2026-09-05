#include "reactor.h"
#include <iostream>
int main() {
    smart_home::Reactor reactor;
    if (!reactor.init("127.0.0.1", 7777)) {
        std::cerr << "init failed" << std::endl;
        return 1;
    }
    std::cout << "listening on 127.0.0.1:7777" << std::endl;
    reactor.run();
    return 0;
}