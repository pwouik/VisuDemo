#include "app.h"
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <iostream>

#define GLAD_GL_IMPLEMENTATION // Necessary for headeronly version.


int main(int argc, char* argv[]) {
    //save seed so if we ever see smth rly cool we can reset it
    unsigned int seed = static_cast<unsigned int>(std::time(nullptr));
    std::srand(seed);

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            std::cout << "usage :\n"
                << "\t--seed <s>\t\tset random seed (default : std::time(nullptr))\n"
                << "\t--nbpts <n>\t\tset number of points (default : " << NBPTS <<")\n";
            return 0;
        } else if (arg == "--seed") {
            if(i+1 == argc) return -1;
            seed = atoi(argv[i+1]);
        } else if (arg == "--nbpts") {
            if(i+1 == argc) return -1;
            //todo
        }
    }
    
    std::srand(seed);
    std::cout << "seed initialized : " << seed << std::endl;

    App app(1600,900);
    app.setupLeapMotion();
    app.run();
    return 0;
}