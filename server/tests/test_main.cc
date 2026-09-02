#include <iostream>
#include "Test.h"

int main() {
    Test test;
    if (!test.run()) {
        std::cerr << "Test failed." << std::endl;
        return 1;
    }

    std::cout << "Test passed." << std::endl;
    return 0;
}
