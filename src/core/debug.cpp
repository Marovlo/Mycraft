#include "debug.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>

uint32_t DebugLog::enabledMask_ = 0;

// Must match DebugCat enum order.
static const char* kCategoryNames[] = {
    "general",
    "entity",
    "mining",
    "input",
    "render",
    "ui",
    "physics",
    "save",
};
static_assert(sizeof(kCategoryNames) / sizeof(kCategoryNames[0])
                  == static_cast<size_t>(DebugCat::_COUNT),
              "Category name table out of sync with DebugCat enum");

const char* DebugLog::categoryName(DebugCat cat) {
    auto idx = static_cast<size_t>(cat);
    return idx < static_cast<size_t>(DebugCat::_COUNT) ? kCategoryNames[idx] : "?";
}

void DebugLog::setEnabled(DebugCat cat, bool on) {
    uint32_t bit = 1u << static_cast<uint8_t>(cat);
    if (on) enabledMask_ |=  bit;
    else    enabledMask_ &= ~bit;
}

void DebugLog::enableAll() {
    enabledMask_ = (1u << static_cast<uint8_t>(DebugCat::_COUNT)) - 1u;
}

void DebugLog::disableAll() {
    enabledMask_ = 0;
}

// Parse comma-separated tokens into a new mask. "all" lights every bit;
// unknown tokens are silently ignored so adding a category never breaks old
// env values.
static uint32_t parseMask(const std::string& spec) {
    if (spec.empty()) return 0;
    if (spec == "all" || spec == "ALL") {
        return (1u << static_cast<uint8_t>(DebugCat::_COUNT)) - 1u;
    }
    uint32_t mask = 0;
    size_t start = 0;
    while (start <= spec.size()) {
        size_t comma = spec.find(',', start);
        std::string token = spec.substr(
            start, comma == std::string::npos ? std::string::npos : comma - start);
        // Trim whitespace.
        size_t a = token.find_first_not_of(" \t");
        size_t b = token.find_last_not_of(" \t");
        if (a != std::string::npos) token = token.substr(a, b - a + 1);
        else                         token.clear();

        if (!token.empty()) {
            for (size_t i = 0; i < static_cast<size_t>(DebugCat::_COUNT); ++i) {
                if (token == kCategoryNames[i]) {
                    mask |= (1u << i);
                    break;
                }
            }
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return mask;
}

void DebugLog::initFromEnv(const char* envOverride) {
    const char* raw = envOverride ? envOverride : std::getenv("VOXEL_DEBUG");
    if (!raw || !*raw) {
        enabledMask_ = 0;
        return;
    }
    enabledMask_ = parseMask(raw);

    // Make stderr line-buffered so VLOG output shows up promptly even when
    // piping to a file — otherwise the kernel buffers our diagnostic logs and
    // they get lost if the process is killed.
    std::setvbuf(stderr, nullptr, _IOLBF, 0);

    if (enabledMask_) {
        std::fprintf(stderr, "[debug] VOXEL_DEBUG=%s → mask=0x%x\n", raw, enabledMask_);
        std::fflush(stderr);
    }
}
