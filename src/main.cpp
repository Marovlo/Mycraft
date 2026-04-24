#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "game/game.h"
#include <iostream>

int main() {
    try {
        Game game;
        game.init();
        game.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
