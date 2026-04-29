#include "texture_animator.h"

#include <stb_image.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace fs = std::filesystem;

// 简单解析 .mcmeta JSON（只提取 frametime 和 frames 数组）
// 不引入完整 JSON 库，手动解析简单结构
static int parseFrametime(const std::string& mcmeta) {
    // 查找 "frametime": N
    auto pos = mcmeta.find("\"frametime\"");
    if (pos == std::string::npos) return 2; // 默认值
    pos = mcmeta.find(':', pos);
    if (pos == std::string::npos) return 2;
    pos++;
    while (pos < mcmeta.size() && (mcmeta[pos] == ' ' || mcmeta[pos] == '\t')) pos++;
    int val = 0;
    while (pos < mcmeta.size() && mcmeta[pos] >= '0' && mcmeta[pos] <= '9') {
        val = val * 10 + (mcmeta[pos] - '0');
        pos++;
    }
    return val > 0 ? val : 2;
}

static std::vector<int> parseFrameOrder(const std::string& mcmeta) {
    std::vector<int> frames;
    // 查找 "frames": [...]
    auto pos = mcmeta.find("\"frames\"");
    if (pos == std::string::npos) return frames; // 空 = 顺序播放
    pos = mcmeta.find('[', pos);
    if (pos == std::string::npos) return frames;
    pos++;
    while (pos < mcmeta.size() && mcmeta[pos] != ']') {
        // 跳过空白和逗号
        while (pos < mcmeta.size() && (mcmeta[pos] == ' ' || mcmeta[pos] == '\t' ||
               mcmeta[pos] == '\n' || mcmeta[pos] == '\r' || mcmeta[pos] == ',')) pos++;
        if (pos >= mcmeta.size() || mcmeta[pos] == ']') break;
        int val = 0;
        bool hasDigit = false;
        while (pos < mcmeta.size() && mcmeta[pos] >= '0' && mcmeta[pos] <= '9') {
            val = val * 10 + (mcmeta[pos] - '0');
            pos++;
            hasDigit = true;
        }
        if (hasDigit) frames.push_back(val);
        else pos++; // 跳过非数字字符
    }
    return frames;
}

bool TextureAnimator::init(const std::string& vanillaBlockDir, TextureAtlas& atlas, uint32_t tileSize) {
    tileSize_ = tileSize;
    animations_.clear();

    if (!fs::exists(vanillaBlockDir)) {
        std::cerr << "TextureAnimator: vanilla block dir not found: " << vanillaBlockDir << "\n";
        return false;
    }

    // 扫描所有 .mcmeta 文件
    for (auto& entry : fs::directory_iterator(vanillaBlockDir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".mcmeta") continue;

        // 对应的 PNG 文件名 = mcmeta 文件名去掉 .mcmeta 后缀
        fs::path pngPath = entry.path();
        pngPath.replace_extension(); // 去掉 .mcmeta，剩下 .png
        if (!fs::exists(pngPath)) continue;

        std::string stem = pngPath.stem().string(); // 如 "water_still"

        // 检查这个纹理是否在我们的图集中
        uint16_t tileIdx = atlas.getTileIndex(stem);
        // getTileIndex 返回 0 表示未找到，但 0 也可能是有效索引（第一个 tile）
        // 通过检查 UV 来确认：如果 tile 0 的名称不是 stem，则说明未找到
        if (tileIdx == 0) {
            // 验证 tile 0 是否确实是这个名称
            glm::vec4 uv = atlas.getTileUV(0);
            // 如果图集中没有这个名称，跳过
            // 简单方法：再次查找确认（getTileIndex 内部用 map 查找）
            // 实际上 getTileIndex 用 unordered_map，找不到返回 0
            // 我们无法区分"真的是 0"和"未找到"，所以只处理已知的动画纹理
            // 已知动画纹理：water_still, lava_still, water_flow, lava_flow 等
            // 如果 tileIdx==0 且 stem 不是常见动画纹理名，跳过
            static const std::vector<std::string> knownAnimated = {
                "water_still", "water_flow", "lava_still", "lava_flow",
                "fire_0", "fire_1", "nether_portal"
            };
            bool isKnown = false;
            for (const auto& k : knownAnimated) {
                if (stem == k) { isKnown = true; break; }
            }
            if (!isKnown) continue;
        }

        // 加载 PNG
        int w, h, ch;
        uint8_t* data = stbi_load(pngPath.string().c_str(), &w, &h, &ch, 4);
        if (!data) continue;

        // 验证宽度 = tileSize，高度 = tileSize * N
        if (static_cast<uint32_t>(w) != tileSize || h < static_cast<int>(tileSize) || h % tileSize != 0) {
            stbi_image_free(data);
            continue;
        }

        int frameCount = h / static_cast<int>(tileSize);
        if (frameCount <= 1) {
            stbi_image_free(data);
            continue; // 单帧不需要动画
        }

        // 读取 mcmeta
        std::ifstream metaFile(entry.path());
        std::string mcmeta((std::istreambuf_iterator<char>(metaFile)),
                            std::istreambuf_iterator<char>());

        int frametime = parseFrametime(mcmeta);
        std::vector<int> frameOrder = parseFrameOrder(mcmeta);

        // 构建动画条目
        AnimatedTexture anim;
        anim.tileName = stem;
        anim.tileIndex = tileIdx;
        anim.frameCount = frameCount;
        anim.frametime = frametime;
        anim.frameOrder = std::move(frameOrder);
        anim.currentFrame = 0;
        anim.tickCounter = 0;

        // 拷贝帧数据
        size_t frameBytes = tileSize * tileSize * 4;
        anim.frameData.resize(frameCount * frameBytes);
        std::memcpy(anim.frameData.data(), data, frameCount * frameBytes);

        stbi_image_free(data);

        animations_.push_back(std::move(anim));
    }

    if (!animations_.empty()) {
        std::cout << "TextureAnimator: loaded " << animations_.size() << " animated textures\n";
    }

    return true;
}

bool TextureAnimator::tick(VulkanEngine& engine, TextureAtlas& atlas) {
    bool anyUpdated = false;
    size_t frameBytes = tileSize_ * tileSize_ * 4;

    for (auto& anim : animations_) {
        anim.tickCounter++;
        if (anim.tickCounter < anim.frametime) continue;

        anim.tickCounter = 0;
        anim.currentFrame++;

        // 确定实际帧索引
        int totalFrames = anim.frameOrder.empty() ? anim.frameCount
                                                   : static_cast<int>(anim.frameOrder.size());
        if (anim.currentFrame >= totalFrames) {
            anim.currentFrame = 0;
        }

        int actualFrame = anim.frameOrder.empty() ? anim.currentFrame
                                                   : anim.frameOrder[anim.currentFrame];

        // 安全检查
        if (actualFrame < 0 || actualFrame >= anim.frameCount) actualFrame = 0;

        // 获取帧像素数据
        const uint8_t* framePixels = anim.frameData.data() + actualFrame * frameBytes;

        // 更新图集中的 tile
        atlas.updateTile(engine, anim.tileIndex, framePixels);
        anyUpdated = true;
    }

    return anyUpdated;
}
