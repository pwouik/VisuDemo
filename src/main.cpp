#include "app.h"
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <iostream>

#define GLAD_GL_IMPLEMENTATION // Necessary for headeronly version.


int main(void) {
    //save seed so if we ever see smth rly cool we can reset it
    unsigned int seed = static_cast<unsigned int>(std::time(nullptr));
    std::srand(seed);
    std::cout << "seed initialized : " << seed << std::endl;

    App app(1600,900);
    app.setupLeapMotion();
    app.run();
    return 0;
}