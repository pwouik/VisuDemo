#include "app.h"
#include <cstdlib>
#include <cstdio>
#include <cmath>
#define GLAD_GL_IMPLEMENTATION // Necessary for headeronly version.


int main(void) {
    DEBUG("creating app ...\n");
    App app(800,800);
    DEBUG("app created");

    unsigned int seed = static_cast<unsigned int>(std::time(nullptr));
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    DEBUG("random seed initialized :")
    DEBUG(seed); //todo read argument from command line to input a seed

    app.run();
    return 0;
}