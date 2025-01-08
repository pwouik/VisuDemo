#include "app.h"
#include <cstdlib>
#include <cstdio>
#include <cmath>
#define GLAD_GL_IMPLEMENTATION // Necessary for headeronly version.


int main(void) {
    DEBUG("creating app ...\n");
    App app(800,800);
    DEBUG("app created");
    app.run();
    return 0;
}