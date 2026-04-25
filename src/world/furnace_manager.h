#pragma once

#include "core/item.h"
#include "core/serialization.h"

#include <unordered_map>
#include <cstdint>

// ========== FurnaceManager ==========
// Manages per-block-position furnace state. Each furnace has:
//   - Input slot (item to smelt)
//   - Fuel slot
//   - Output slot
//   - Smelting progress (ticks)
//   - Fuel burn remaining (ticks)
//   - Total burn time of current fuel (for GUI progress display)
//
// MC furnace behavior:
//   - Fuel is consumed when smelting starts (or continues)
//   - Smelting takes 200 ticks (10 seconds) per item
//   - Fuel burns independently once lit (even if input is removed)
//   - Furnace continues smelting while fuel remains, even when GUI is closed

class FurnaceManager {
public:
    struct FurnaceData {
        ItemStack inputSlot;
        ItemStack fuelSlot;
        ItemStack outputSlot;

        int smeltProgress = 0;      // 当前冶炼进度 (0 ~ smeltTotalTicks)
        int smeltTotalTicks = 200;   // 当前配方所需总tick数

        int fuelBurnRemaining = 0;  // 当前燃料剩余燃烧tick数
        int fuelBurnTotal = 0;      // 当前燃料总燃烧tick数（用于GUI火焰进度）

        bool isBurning() const { return fuelBurnRemaining > 0; }
        bool isSmelting() const { return smeltProgress > 0; }

        // GUI进度比例 [0.0, 1.0]
        float smeltProgressRatio() const {
            return smeltTotalTicks > 0
                ? static_cast<float>(smeltProgress) / static_cast<float>(smeltTotalTicks)
                : 0.0f;
        }
        float fuelProgressRatio() const {
            return fuelBurnTotal > 0
                ? static_cast<float>(fuelBurnRemaining) / static_cast<float>(fuelBurnTotal)
                : 0.0f;
        }
    };

    // 获取或创建指定位置的熔炉数据
    FurnaceData& getOrCreate(int x, int y, int z);

    // 获取熔炉数据（不存在返回nullptr）
    FurnaceData* get(int x, int y, int z);
    const FurnaceData* get(int x, int y, int z) const;

    // 移除熔炉（方块被破坏时调用），返回内容用于掉落
    FurnaceData remove(int x, int y, int z);

    bool has(int x, int y, int z) const;

    // 每tick更新所有活跃熔炉的冶炼逻辑
    void tick();

    // 序列化/反序列化
    void serialize(BinaryWriter& w) const;
    void deserialize(BinaryReader& r);

    void clear() { furnaces_.clear(); }
    size_t count() const { return furnaces_.size(); }

private:
    static uint64_t packKey(int x, int y, int z) {
        uint64_t ux = static_cast<uint64_t>(static_cast<uint32_t>(x));
        uint64_t uy = static_cast<uint64_t>(static_cast<uint32_t>(y));
        uint64_t uz = static_cast<uint64_t>(static_cast<uint32_t>(z));
        return (ux) | (uy << 32) | (uz << 48);
    }

    struct KeyHash {
        size_t operator()(uint64_t k) const {
            return static_cast<size_t>(k * 11400714819323198485ULL);
        }
    };

    // 单个熔炉的tick逻辑
    void tickFurnace(FurnaceData& data);

    std::unordered_map<uint64_t, FurnaceData, KeyHash> furnaces_;
};
