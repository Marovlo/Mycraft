#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "game/game.h"
#include "core/debug.h"
#include <iostream>
#include <cstring>

// Parse --debug[=cats] from argv. Command-line flag wins over env variable.
//   --debug             → VOXEL_DEBUG=all (all categories)
//   --debug=entity,ui   → just those categories
static const char* extractDebugFlag(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (!argv[i]) continue;
        if (std::strcmp(argv[i], "--debug") == 0) return "all";
        if (std::strncmp(argv[i], "--debug=", 8) == 0) return argv[i] + 8;
    }
    return nullptr;
}

int main(int argc, char** argv) {
    DebugLog::initFromEnv(extractDebugFlag(argc, argv));

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
