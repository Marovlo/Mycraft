#include "day_night_cycle.h"
#include <cmath>
#include <algorithm>

void DayNightCycle::tick() {
    totalTicks_++;
    time_ = totalTicks_ % 24000;
    totalDays_ = totalTicks_ / 24000;
}

float DayNightCycle::getSkyLightFactor() const {
    // MC 原版 skyLight 因子：
    // 白天 (1000-12000): 1.0
    // 日落 (12000-13000): 1.0 → 0.25 线性过渡
    // 夜晚 (13000-23000): 0.25
    // 日出 (23000-24000, 0-1000): 0.25 → 1.0 线性过渡
    if (time_ >= 1000 && time_ < 12000) {
        return 1.0f;
    } else if (time_ >= 12000 && time_ < 13000) {
        float t = static_cast<float>(time_ - 12000) / 1000.0f;
        return 1.0f - t * 0.75f;  // 1.0 → 0.25
    } else if (time_ >= 13000 && time_ < 23000) {
        return 0.25f;
    } else {
        // 23000-24000 或 0-1000
        uint32_t t = (time_ >= 23000) ? (time_ - 23000) : (time_ + 1000);
        float frac = static_cast<float>(t) / 2000.0f;
        return 0.25f + frac * 0.75f;  // 0.25 → 1.0
    }
}

glm::vec3 DayNightCycle::getSkyColor() const {
    // 白天：浅蓝色 (0.53, 0.68, 1.0)
    // 日落：橙色 (0.85, 0.45, 0.25)
    // 夜晚：深蓝色 (0.01, 0.01, 0.03)
    // 日出：橙粉色 (0.75, 0.40, 0.30)
    static const glm::vec3 DAY_SKY    = {0.53f, 0.68f, 1.0f};
    static const glm::vec3 SUNSET_SKY = {0.85f, 0.45f, 0.25f};
    static const glm::vec3 NIGHT_SKY  = {0.01f, 0.01f, 0.03f};
    static const glm::vec3 SUNRISE_SKY = {0.75f, 0.40f, 0.30f};

    if (time_ >= 1000 && time_ < 11500) {
        return DAY_SKY;
    } else if (time_ >= 11500 && time_ < 12500) {
        float t = static_cast<float>(time_ - 11500) / 1000.0f;
        return glm::mix(DAY_SKY, SUNSET_SKY, t);
    } else if (time_ >= 12500 && time_ < 13500) {
        float t = static_cast<float>(time_ - 12500) / 1000.0f;
        return glm::mix(SUNSET_SKY, NIGHT_SKY, t);
    } else if (time_ >= 13500 && time_ < 22500) {
        return NIGHT_SKY;
    } else if (time_ >= 22500 && time_ < 23500) {
        float t = static_cast<float>(time_ - 22500) / 1000.0f;
        return glm::mix(NIGHT_SKY, SUNRISE_SKY, t);
    } else {
        // 23500-24000 或 0-1000
        uint32_t elapsed = (time_ >= 23500) ? (time_ - 23500) : (time_ + 500);
        float t = static_cast<float>(elapsed) / 1500.0f;
        return glm::mix(SUNRISE_SKY, DAY_SKY, std::min(t, 1.0f));
    }
}

glm::vec3 DayNightCycle::getFogColor() const {
    // 雾色跟随天空色，但稍微偏灰
    glm::vec3 sky = getSkyColor();
    return glm::mix(sky, glm::vec3(0.5f, 0.5f, 0.5f), 0.15f);
}

bool DayNightCycle::isDay() const {
    return time_ >= 1000 && time_ < 13000;
}

bool DayNightCycle::isNight() const {
    return time_ >= 13000 || time_ < 1000;
}

float DayNightCycle::getSunAngle() const {
    // 太阳从东方升起(0°)，正午在头顶(90°)，西方落下(180°)
    return static_cast<float>(time_) / 24000.0f * 360.0f;
}
