#include "sky_renderer.h"

#include <glm/gtc/constants.hpp>
#include <cmath>

void SkyRenderer::render(VkCommandBuffer cmd, VulkanEngine& engine, const DayNightCycle& dayNight) {
    // 绑定天空管线
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine.getSkyPipeline());

    // 绑定 descriptor set（需要 UBO 中的 view/proj 矩阵）
    auto& frame = engine.getCurrentFrame();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        engine.getSkyPipelineLayout(), 0, 1, &frame.descriptorSet, 0, nullptr);

    // 计算天空参数并通过 push constant 传入
    SkyPushConstants params = computeSkyParams(dayNight);
    vkCmdPushConstants(cmd, engine.getSkyPipelineLayout(),
        VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SkyPushConstants), &params);

    // 绘制全屏三角形（3 个顶点，无顶点缓冲）
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

glm::vec3 SkyRenderer::computeSunDirection(float sunAngle) const {
    // sunAngle: 0-360度
    // MC 中太阳从东方升起，在南方天空运行
    // 0° = 日出（东方），90° = 正午（头顶），180° = 日落（西方），270° = 午夜（脚下）
    float rad = glm::radians(sunAngle - 90.0f);  // 偏移使 0° 对应东方地平线
    float y = std::cos(rad);   // 垂直分量
    float xz = std::sin(rad);  // 水平分量
    // 太阳在南方天空运行（-Z 方向）
    return glm::normalize(glm::vec3(xz, y, -0.3f * xz));
}

SkyPushConstants SkyRenderer::computeSkyParams(const DayNightCycle& dayNight) const {
    SkyPushConstants params{};

    // 天空颜色
    glm::vec3 skyColor = dayNight.getSkyColor();
    // 天顶比地平线更深
    params.skyColorTop = glm::vec4(skyColor * 0.7f, 1.0f);
    params.skyColorHorizon = glm::vec4(skyColor, 1.0f);

    // 太阳/月亮方向
    float sunAngle = dayNight.getSunAngle();
    glm::vec3 sunDir = computeSunDirection(sunAngle);
    glm::vec3 moonDir = -sunDir;  // 月亮在太阳对面

    params.sunDir = glm::vec4(sunDir, 0.015f);   // w = 太阳大小
    params.moonDir = glm::vec4(moonDir, 0.012f);  // w = 月亮大小（略小）

    // 太阳/月亮/星空可见度
    uint32_t time = dayNight.getTime();
    float sunAlpha = 0.0f;
    float moonAlpha = 0.0f;
    float starAlpha = 0.0f;

    // 白天：太阳可见
    if (time >= 500 && time < 12500) {
        sunAlpha = 1.0f;
        // 日出/日落时渐变
        if (time < 1500) sunAlpha = static_cast<float>(time - 500) / 1000.0f;
        else if (time > 11500) sunAlpha = static_cast<float>(12500 - time) / 1000.0f;
    }

    // 夜晚：月亮和星星可见
    if (time >= 13000 || time < 500) {
        moonAlpha = 1.0f;
        starAlpha = 1.0f;
        // 过渡
        if (time >= 13000 && time < 14000) {
            float t = static_cast<float>(time - 13000) / 1000.0f;
            moonAlpha = t;
            starAlpha = t;
        }
    } else if (time >= 12000 && time < 13000) {
        // 日落过渡：星星逐渐出现
        float t = static_cast<float>(time - 12000) / 1000.0f;
        starAlpha = t * 0.5f;
        moonAlpha = t * 0.3f;
    } else if (time < 1500 && time >= 500) {
        // 日出过渡：星星逐渐消失
        float t = static_cast<float>(1500 - time) / 1000.0f;
        starAlpha = t * 0.5f;
        moonAlpha = t * 0.3f;
    }

    params.sunMoonAlpha = glm::vec4(sunAlpha, moonAlpha, starAlpha, 0.0f);

    return params;
}
