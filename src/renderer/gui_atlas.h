#pragma once

#include "engine/vulkan_engine.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

// GUI 纹理图集 — 加载原版 MC 的 GUI 精灵图（尺寸各异）到一张大纹理中。
// 与方块图集不同，GUI 精灵图尺寸不统一（hotbar 182x22, heart 9x9 等），
// 使用简单的行优先矩形装箱算法（shelf packing）。
//
// 用法：
//   guiAtlas.build(engine, assetDir);
//   auto sprite = guiAtlas.getSprite("hotbar");
//   uiRenderer.drawGuiSprite(sprite, x, y, w, h);

struct GuiSprite {
    float u0, v0, u1, v1;  // 归一化 UV 坐标
    int   pixelW, pixelH;  // 原始像素尺寸
};

class GuiAtlas {
public:
    // 从 assets 目录构建 GUI 图集
    // 扫描 minecraft_vanilla/textures/gui/sprites/hud/ 和 container/ 等目录
    bool build(VulkanEngine& engine, const std::string& assetDir);

    void destroy(VulkanEngine& engine);

    // 按名称查找精灵（名称 = 相对路径去掉扩展名，如 "hud/hotbar"）
    const GuiSprite& getSprite(const std::string& name) const;

    // 获取 GPU 纹理资源
    VkImageView getImageView() const { return image_.imageView; }
    VkSampler getSampler() const { return sampler_; }

    bool isBuilt() const { return built_; }

private:
    AllocatedImage image_;
    VkSampler sampler_ = VK_NULL_HANDLE;
    bool built_ = false;

    std::unordered_map<std::string, GuiSprite> sprites_;
    GuiSprite fallback_{0, 0, 0, 0, 0, 0};  // 找不到时返回的空精灵
};
