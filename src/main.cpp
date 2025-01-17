#include "app.h"
#include <cstdlib>
#include <cstdio>
#include <cmath>
#define GLAD_GL_IMPLEMENTATION // Necessary for headeronly version.


int main(void) {
    App app(1600,900);
    app.setupLeapMotion();
    app.run();
    return 0;
}