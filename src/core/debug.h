#pragma once

#include <cstdio>
#include <cstdint>
#include <string_view>

// ============================================================================
// Persistent, category-filtered debug logging.
//
// Goal: avoid the edit→run→edit→run loop of sprinkling printfs while debugging.
// Logs stay compiled in (near-zero overhead when disabled — one atomic bitmask
// check) and are filtered at runtime by category.
//
// Usage:
//   #include "core/debug.h"
//   VLOG(DebugCat::Entity, "spawn %d at (%.2f,%.2f,%.2f)", id, x, y, z);
//
// Enabling at runtime:
//   Environment variable VOXEL_DEBUG:
//     unset / empty  → everything OFF
//     "all"          → all categories ON
//     "entity,ui"    → just those categories ON
//
//   Command-line flag passed via run.sh --debug sets VOXEL_DEBUG=all for that
//   launch. Individual categories can be narrowed with --debug=entity,ui.
//
// Adding a category: extend DebugCat, update kCategoryNames in debug.cpp.
// ============================================================================

enum class DebugCat : uint8_t {
    General = 0,   // catch-all
    Entity,        // ItemEntity / EntityManager lifecycle
    Mining,        // BlockInteraction progression
    Input,         // keyboard / mouse event tracing
    Render,        // renderer frame-by-frame info
    UI,            // hud / inventory display
    Physics,       // player / entity collision
    Save,          // save/load operations
    _COUNT
};

class DebugLog {
public:
    // Parse VOXEL_DEBUG from environment. Call once at startup before any
    // VLOG macros. Passing nullptr reads getenv("VOXEL_DEBUG").
    static void initFromEnv(const char* envOverride = nullptr);

    // True if the given category is enabled for logging.
    static bool enabled(DebugCat cat) {
        return (enabledMask_ >> static_cast<uint8_t>(cat)) & 1u;
    }

    // Manually flip categories (e.g. F3 key could toggle them live).
    static void setEnabled(DebugCat cat, bool on);
    static void enableAll();
    static void disableAll();

    // Category name for log prefixes.
    static const char* categoryName(DebugCat cat);

private:
    // Bitmask, bit i = 1 → category i enabled.
    static uint32_t enabledMask_;
};

// The VLOG macro short-circuits the enabled check before evaluating the format
// arguments, so disabled logs cost one mask test and a branch.
#define VLOG(cat, ...)                                                         \
    do {                                                                       \
        if (DebugLog::enabled(cat)) {                                          \
            std::fprintf(stderr, "[%s] ", DebugLog::categoryName(cat));        \
            std::fprintf(stderr, __VA_ARGS__);                                 \
            std::fputc('\n', stderr);                                          \
        }                                                                      \
    } while (0)
