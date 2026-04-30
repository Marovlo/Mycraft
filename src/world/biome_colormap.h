#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <cstdint>

// MC原版生物群系颜色映射系统
// 加载 colormap/grass.png 和 colormap/foliage.png (256x256)
// 根据生物群系的温度和湿度查表获取 tint 颜色
class BiomeColorMap {
public:
    // 加载 colormap 图片（从 assets/minecraft_vanilla/textures/colormap/ 目录）
    bool load(const std::string& assetDir);

    // 根据温度和湿度查询草颜色 (temp: 0.0-1.0, humidity: 0.0-1.0)
    glm::vec3 getGrassColor(float temperature, float humidity) const;

    // 根据温度和湿度查询树叶颜色
    glm::vec3 getFoliageColor(float temperature, float humidity) const;

    // MC原版云杉叶固定颜色 (硬编码)
    static glm::vec3 getSpruceColor() { return glm::vec3(97.0f/255.0f, 153.0f/255.0f, 97.0f/255.0f); }

    // 便捷方法：根据生物群系类型获取草颜色
    // MC原版各生物群系的温度/湿度参数：
    //   Plains:  temp=0.8, humidity=0.4
    //   Forest:  temp=0.7, humidity=0.8
    //   Desert:  temp=2.0, humidity=0.0 (clamped to 1.0)
    //   Snowy:   temp=0.0, humidity=0.5
    glm::vec3 getGrassColorForBiome(int biomeType) const;
    glm::vec3 getFoliageColorForBiome(int biomeType) const;

private:
    // 256x256 RGBA 像素数据
    std::vector<uint8_t> grassPixels_;
    std::vector<uint8_t> foliagePixels_;
    int grassWidth_ = 0, grassHeight_ = 0;
    int foliageWidth_ = 0, foliageHeight_ = 0;

    // 从 colormap 查表：x = (1-temp)*255, y = (1-humidity*temp)*255 (MC原版公式)
    glm::vec3 sampleColormap(const std::vector<uint8_t>& pixels, int width, int height,
                             float temperature, float humidity) const;
};
