#pragma once

#include "texture_atlas.h"
#include "engine/vulkan_engine.h"
#include <vector>
#include <string>
#include <cstdint>

// 单个动画纹理的定义
struct AnimatedTexture {
    std::string tileName;           // 图集中的 tile 名称（如 "water_still"）
    uint16_t tileIndex = 0;         // 图集中的 tile 索引
    int frameCount = 0;             // 总帧数
    int frametime = 2;              // 每帧持续的游戏 tick 数
    std::vector<int> frameOrder;    // 帧播放顺序（空 = 顺序播放 0,1,2,...）
    std::vector<uint8_t> frameData; // 所有帧的 RGBA 像素数据（frameCount * tileSize * tileSize * 4）

    // 运行时状态
    int currentFrame = 0;           // 当前帧在 frameOrder 中的索引
    int tickCounter = 0;            // 当前帧已经过的 tick 数
};

// 纹理动画管理器
// 负责加载多帧纹理、按 tick 切换帧、更新图集
class TextureAnimator {
public:
    // 初始化：扫描原版资源目录，加载所有动画纹理
    // vanillaBlockDir: 原版方块纹理目录（含 .mcmeta 文件）
    // tileSize: 单帧尺寸（16）
    bool init(const std::string& vanillaBlockDir, TextureAtlas& atlas, uint32_t tileSize = 16);

    // 每游戏 tick 调用一次，推进动画帧
    // 返回 true 表示有纹理被更新（需要刷新渲染）
    bool tick(VulkanEngine& engine, TextureAtlas& atlas);

    // 获取动画纹理数量
    size_t count() const { return animations_.size(); }

private:
    std::vector<AnimatedTexture> animations_;
    uint32_t tileSize_ = 16;
};
