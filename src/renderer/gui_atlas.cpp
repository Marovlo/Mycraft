#include "gui_atlas.h"

#include <stb_image.h>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <cstring>

namespace fs = std::filesystem;

// 收集目录下所有 PNG 文件（递归），返回 {相对路径名, 绝对路径}
static void collectGuiPngs(const std::string& baseDir, const std::string& relPrefix,
                           std::vector<std::pair<std::string, std::string>>& out) {
    if (!fs::exists(baseDir)) return;
    for (auto& entry : fs::recursive_directory_iterator(baseDir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".png") continue;
        // 跳过 .mcmeta 对应的动画帧文件
        std::string absPath = entry.path().string();
        // 计算相对名称：去掉 baseDir 前缀和 .png 后缀
        std::string rel = entry.path().lexically_relative(baseDir).string();
        // 去掉 .png 后缀
        if (rel.size() > 4) rel = rel.substr(0, rel.size() - 4);
        // 加上前缀
        std::string name = relPrefix.empty() ? rel : relPrefix + "/" + rel;
        out.push_back({name, absPath});
    }
}

// 简单的 shelf packing 算法
struct ShelfPacker {
    int atlasW, atlasH;
    int curX = 0, curY = 0, shelfH = 0;

    ShelfPacker(int w, int h) : atlasW(w), atlasH(h) {}

    // 尝试放置一个矩形，返回左上角坐标，失败返回 {-1,-1}
    std::pair<int,int> pack(int w, int h) {
        if (curX + w > atlasW) {
            // 换行
            curX = 0;
            curY += shelfH;
            shelfH = 0;
        }
        if (curY + h > atlasH) return {-1, -1};  // 放不下
        int px = curX, py = curY;
        curX += w;
        if (h > shelfH) shelfH = h;
        return {px, py};
    }
};

bool GuiAtlas::build(VulkanEngine& engine, const std::string& assetDir) {
    sprites_.clear();

    // 收集所有 GUI 精灵图
    std::vector<std::pair<std::string, std::string>> pngs;  // {name, absPath}

    std::string guiSpritesDir = assetDir + "/minecraft_vanilla/textures/gui/sprites";
    collectGuiPngs(guiSpritesDir + "/hud", "hud", pngs);
    collectGuiPngs(guiSpritesDir + "/container", "container", pngs);
    collectGuiPngs(guiSpritesDir + "/widget", "widget", pngs);

    // 也收集 gui/ 根目录下的容器贴图（inventory.png, crafting_table.png 等）
    std::string guiContainerDir = assetDir + "/minecraft_vanilla/textures/gui/container";
    if (fs::exists(guiContainerDir)) {
        for (auto& entry : fs::directory_iterator(guiContainerDir)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".png") continue;
            std::string name = "gui_container/" + entry.path().stem().string();
            pngs.push_back({name, entry.path().string()});
        }
    }

    // 收集标题图片
    std::string titleDir = assetDir + "/minecraft_vanilla/textures/gui/title";
    if (fs::exists(titleDir)) {
        for (auto& entry : fs::directory_iterator(titleDir)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".png") continue;
            std::string name = "title/" + entry.path().stem().string();
            pngs.push_back({name, entry.path().string()});
        }
    }

    // 按名称排序保证确定性
    std::sort(pngs.begin(), pngs.end());

    if (pngs.empty()) {
        std::cerr << "GuiAtlas: no GUI sprites found\n";
        return false;
    }

    // 第一遍：加载所有图片获取尺寸
    struct LoadedImg {
        std::string name;
        uint8_t* data;
        int w, h;
    };
    std::vector<LoadedImg> images;
    images.reserve(pngs.size());

    int totalArea = 0;
    for (auto& [name, path] : pngs) {
        int w, h, ch;
        uint8_t* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
        if (!data) {
            std::cerr << "GuiAtlas: failed to load " << path << "\n";
            continue;
        }
        images.push_back({name, data, w, h});
        totalArea += w * h;
    }

    // 按高度降序排序（shelf packing 效率更高）
    std::sort(images.begin(), images.end(), [](const LoadedImg& a, const LoadedImg& b) {
        return a.h > b.h;
    });

    // 计算图集尺寸（2 的幂次，足够容纳所有精灵）
    int atlasSize = 256;
    while (atlasSize * atlasSize < totalArea * 2) {  // 留 2x 余量
        atlasSize *= 2;
    }
    // 上限 4096
    atlasSize = std::min(atlasSize, 4096);

    // 尝试装箱，如果放不下就扩大
    std::vector<uint8_t> atlasPixels;
    bool packed = false;

    while (atlasSize <= 4096) {
        ShelfPacker packer(atlasSize, atlasSize);
        sprites_.clear();
        bool allFit = true;

        for (auto& img : images) {
            auto [px, py] = packer.pack(img.w, img.h);
            if (px < 0) {
                allFit = false;
                break;
            }
            float invSize = 1.0f / static_cast<float>(atlasSize);
            GuiSprite sprite;
            sprite.u0 = px * invSize;
            sprite.v0 = py * invSize;
            sprite.u1 = (px + img.w) * invSize;
            sprite.v1 = (py + img.h) * invSize;
            sprite.pixelW = img.w;
            sprite.pixelH = img.h;
            sprites_[img.name] = sprite;
        }

        if (allFit) {
            // 创建像素数据
            atlasPixels.resize(atlasSize * atlasSize * 4, 0);
            for (auto& img : images) {
                auto& sp = sprites_[img.name];
                int px = static_cast<int>(sp.u0 * atlasSize);
                int py = static_cast<int>(sp.v0 * atlasSize);
                for (int y = 0; y < img.h; y++) {
                    std::memcpy(&atlasPixels[((py + y) * atlasSize + px) * 4],
                                &img.data[y * img.w * 4],
                                img.w * 4);
                }
            }
            packed = true;
            break;
        }

        atlasSize *= 2;
    }

    // 释放所有加载的图片
    for (auto& img : images) {
        stbi_image_free(img.data);
    }

    if (!packed) {
        std::cerr << "GuiAtlas: failed to pack all sprites (too many/too large)\n";
        return false;
    }

    // 上传到 GPU
    image_ = engine.uploadTexture(atlasPixels.data(), atlasSize, atlasSize, 4);

    // 创建 nearest-neighbor 采样器（像素风格）
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCreateSampler(engine.getDevice(), &samplerInfo, nullptr, &sampler_);

    built_ = true;
    std::cout << "GuiAtlas: loaded " << sprites_.size() << " sprites ("
              << atlasSize << "x" << atlasSize << " px)\n";

    return true;
}

void GuiAtlas::destroy(VulkanEngine& engine) {
    if (sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(engine.getDevice(), sampler_, nullptr);
        sampler_ = VK_NULL_HANDLE;
    }
    if (built_) {
        engine.destroyTexture(image_);
        built_ = false;
    }
    sprites_.clear();
}

const GuiSprite& GuiAtlas::getSprite(const std::string& name) const {
    auto it = sprites_.find(name);
    if (it != sprites_.end()) return it->second;
    return fallback_;
}
