#pragma once

#include <cstdint>
#include <glm/glm.hpp>

// MC 昼夜循环：一天 = 24000 ticks = 20 分钟
// 时间段：日出(0-1000) → 白天(1000-12000) → 日落(12000-13000) → 夜晚(13000-23000) → 日出(23000-24000)
class DayNightCycle {
public:
    void tick();

    // 获取当前时间 (0-23999)
    uint32_t getTime() const { return time_; }
    void setTime(uint32_t t) { time_ = t % 24000; }

    // 获取总天数
    uint32_t getDay() const { return totalDays_; }

    // 天空光照因子 (0.0 = 完全黑夜, 1.0 = 完全白天)
    // 用于乘以 skyLight 值
    float getSkyLightFactor() const;

    // 天空颜色（用于 fogColor / clearColor）
    glm::vec3 getSkyColor() const;

    // 雾颜色
    glm::vec3 getFogColor() const;

    // 是否是白天（亮度足够高，被动生物可以生成）
    bool isDay() const;

    // 是否是夜晚（亮度足够低，敌对生物可以生成）
    bool isNight() const;

    // 太阳角度 (0-360度，用于渲染太阳/月亮位置)
    float getSunAngle() const;

    // 序列化
    uint32_t getTotalTicks() const { return totalTicks_; }
    void setTotalTicks(uint32_t t) { totalTicks_ = t; time_ = t % 24000; totalDays_ = t / 24000; }

private:
    uint32_t time_ = 1000;       // 从白天开始
    uint32_t totalTicks_ = 1000;
    uint32_t totalDays_ = 0;
};
