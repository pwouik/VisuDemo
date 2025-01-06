#include "app.h"
#include <cstdlib>
#include <cstdio>
#include <cmath>
#define GLAD_GL_IMPLEMENTATION // Necessary for headeronly version.


int main(void) {
    App app(800,800);
    app.run();
    return 0;
}