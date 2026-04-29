#pragma once

#include "engine/vulkan_engine.h"
#include "world/day_night_cycle.h"
#include <glm/glm.hpp>

// 天空渲染 Push Constants（必须与 sky.frag 中的布局匹配）
struct SkyPushConstants {
    glm::vec4 skyColorTop;      // 天顶颜色
    glm::vec4 skyColorHorizon;  // 地平线颜色
    glm::vec4 sunDir;           // xyz = 太阳方向, w = sunSize
    glm::vec4 moonDir;          // xyz = 月亮方向, w = moonSize
    glm::vec4 sunMoonAlpha;     // x = sunAlpha, y = moonAlpha, z = starAlpha, w = unused
};

// 天空渲染器
// 使用全屏三角形 + 片段着色器渲染天空渐变、太阳、月亮和星空
class SkyRenderer {
public:
    // 渲染天空（在不透明方块之前调用）
    void render(VkCommandBuffer cmd, VulkanEngine& engine, const DayNightCycle& dayNight);

private:
    // 根据昼夜循环计算天空参数
    SkyPushConstants computeSkyParams(const DayNightCycle& dayNight) const;

    // 计算太阳/月亮方向（基于角度）
    glm::vec3 computeSunDirection(float sunAngle) const;
};
