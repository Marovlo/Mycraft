#include "biome_colormap.h"

#include <iostream>
#include <algorithm>

#include <stb_image.h>

bool BiomeColorMap::load(const std::string& assetDir) {
    std::string grassPath = assetDir + "/minecraft_vanilla/textures/colormap/grass.png";
    std::string foliagePath = assetDir + "/minecraft_vanilla/textures/colormap/foliage.png";

    // 加载 grass colormap
    int channels;
    uint8_t* grassData = stbi_load(grassPath.c_str(), &grassWidth_, &grassHeight_, &channels, 4);
    if (!grassData) {
        std::cerr << "[BiomeColorMap] Failed to load: " << grassPath << "\n";
        return false;
    }
    grassPixels_.assign(grassData, grassData + grassWidth_ * grassHeight_ * 4);
    stbi_image_free(grassData);

    // 加载 foliage colormap
    uint8_t* foliageData = stbi_load(foliagePath.c_str(), &foliageWidth_, &foliageHeight_, &channels, 4);
    if (!foliageData) {
        std::cerr << "[BiomeColorMap] Failed to load: " << foliagePath << "\n";
        return false;
    }
    foliagePixels_.assign(foliageData, foliageData + foliageWidth_ * foliageHeight_ * 4);
    stbi_image_free(foliageData);

    std::cout << "[BiomeColorMap] Loaded grass (" << grassWidth_ << "x" << grassHeight_
              << ") and foliage (" << foliageWidth_ << "x" << foliageHeight_ << ") colormaps\n";
    return true;
}

glm::vec3 BiomeColorMap::sampleColormap(const std::vector<uint8_t>& pixels, int width, int height,
                                         float temperature, float humidity) const {
    // MC原版公式：将温度和湿度映射到 colormap 坐标
    // temperature 和 humidity 都 clamp 到 [0, 1]
    temperature = std::clamp(temperature, 0.0f, 1.0f);
    humidity = std::clamp(humidity, 0.0f, 1.0f);
    // MC原版：humidity *= temperature (调整后的湿度)
    humidity *= temperature;

    // colormap 坐标：x = (1 - temperature) * 255, y = (1 - humidity) * 255
    int x = static_cast<int>((1.0f - temperature) * 255.0f);
    int y = static_cast<int>((1.0f - humidity) * 255.0f);

    x = std::clamp(x, 0, width - 1);
    y = std::clamp(y, 0, height - 1);

    int idx = (y * width + x) * 4;
    return glm::vec3(
        pixels[idx + 0] / 255.0f,
        pixels[idx + 1] / 255.0f,
        pixels[idx + 2] / 255.0f
    );
}

glm::vec3 BiomeColorMap::getGrassColor(float temperature, float humidity) const {
    if (grassPixels_.empty()) return glm::vec3(1.0f);
    return sampleColormap(grassPixels_, grassWidth_, grassHeight_, temperature, humidity);
}

glm::vec3 BiomeColorMap::getFoliageColor(float temperature, float humidity) const {
    if (foliagePixels_.empty()) return glm::vec3(1.0f);
    return sampleColormap(foliagePixels_, foliageWidth_, foliageHeight_, temperature, humidity);
}

glm::vec3 BiomeColorMap::getGrassColorForBiome(int biomeType) const {
    // MC原版各生物群系温度/湿度参数
    // 0=Plains, 1=Forest, 2=Desert, 3=Snowy
    switch (biomeType) {
        case 0: return getGrassColor(0.8f, 0.4f);   // Plains
        case 1: return getGrassColor(0.7f, 0.8f);   // Forest
        case 2: return getGrassColor(2.0f, 0.0f);   // Desert (clamped to 1.0)
        case 3: return getGrassColor(0.0f, 0.5f);   // Snowy
        default: return getGrassColor(0.8f, 0.4f);  // Default = Plains
    }
}

glm::vec3 BiomeColorMap::getFoliageColorForBiome(int biomeType) const {
    switch (biomeType) {
        case 0: return getFoliageColor(0.8f, 0.4f);   // Plains
        case 1: return getFoliageColor(0.7f, 0.8f);   // Forest
        case 2: return getFoliageColor(2.0f, 0.0f);   // Desert
        case 3: return getFoliageColor(0.0f, 0.5f);   // Snowy
        default: return getFoliageColor(0.8f, 0.4f);
    }
}
