#pragma once

#include <cstdint>

// Fixed-rate game tick clock (20 TPS = 50ms per tick, same as Minecraft).
// Decouples game logic from render framerate.
// Usage:
//   while (!quit) {
//       int ticks = clock.advance(currentTime);
//       for (int i = 0; i < ticks; i++) gameTick();
//       render(clock.getPartialTick());
//   }

class TickClock {
public:
    static constexpr double TICK_RATE = 20.0;
    static constexpr double TICK_DURATION = 1.0 / TICK_RATE;
    static constexpr int MAX_TICKS_PER_FRAME = 10;  // prevent spiral of death

    // Call once per frame with current time (seconds).
    // Returns number of ticks to execute this frame (usually 0 or 1).
    int advance(double currentTime) {
        if (lastTime_ == 0.0) {
            lastTime_ = currentTime;
            return 0;
        }

        double elapsed = currentTime - lastTime_;
        lastTime_ = currentTime;
        accumulator_ += elapsed;

        int ticks = 0;
        while (accumulator_ >= TICK_DURATION && ticks < MAX_TICKS_PER_FRAME) {
            accumulator_ -= TICK_DURATION;
            totalTicks_++;
            ticks++;
        }

        // Clamp accumulator to prevent unbounded growth if severely lagging
        if (accumulator_ > TICK_DURATION * MAX_TICKS_PER_FRAME) {
            accumulator_ = 0.0;
        }

        partialTick_ = static_cast<float>(accumulator_ / TICK_DURATION);
        return ticks;
    }

    // Interpolation factor [0, 1) between last tick and next tick.
    // Use for smooth visual interpolation in render().
    float getPartialTick() const { return partialTick_; }

    // Total game ticks elapsed since start
    uint64_t getTotalTicks() const { return totalTicks_; }

    // World time (wraps every 24000 ticks = 1 MC day)
    uint64_t getWorldTime() const { return totalTicks_ % 24000; }

    // Reset the clock (used when entering a world to avoid tick burst)
    void reset(double currentTime) {
        lastTime_ = currentTime;
        accumulator_ = 0.0;
        partialTick_ = 0.0f;
    }

private:
    double lastTime_ = 0.0;
    double accumulator_ = 0.0;
    uint64_t totalTicks_ = 0;
    float partialTick_ = 0.0f;
};
