#pragma once

#include "core/item.h"
#include <vector>
#include <unordered_map>

// 冶炼配方：输入物品 → 输出物品 + 经验值 + 冶炼时间(tick)
// MC原版冶炼时间统一为200 ticks (10秒)
struct SmeltingRecipe {
    ItemId input;           // 输入物品ID
    ItemStack output;       // 输出物品
    float experience;       // 经验值（预留，当前未使用）
    int smeltTimeTicks;     // 冶炼所需tick数（默认200 = 10秒）
};

// 燃料定义：物品ID → 燃烧时间(tick)
struct FuelEntry {
    ItemId item;
    int burnTimeTicks;      // 燃烧持续tick数
};

// 冶炼配方注册表（单例）
class SmeltingRegistry {
public:
    static SmeltingRegistry& instance();

    // 注册一条冶炼配方
    void registerRecipe(ItemId input, ItemStack output, float xp = 0.0f,
                        int smeltTime = 200);

    // 注册燃料
    void registerFuel(ItemId item, int burnTimeTicks);

    // 查找配方：给定输入物品，返回配方指针（nullptr = 不可冶炼）
    const SmeltingRecipe* findRecipe(ItemId input) const;

    // 查找燃料燃烧时间：返回0表示不是燃料
    int getFuelBurnTime(ItemId item) const;

    // 判断是否为燃料
    bool isFuel(ItemId item) const;

    // 注册所有默认配方和燃料
    void registerDefaults();

    const std::vector<SmeltingRecipe>& allRecipes() const { return recipes_; }

private:
    SmeltingRegistry() = default;
    std::vector<SmeltingRecipe> recipes_;
    std::unordered_map<ItemId, int> recipeIndex_;   // input → index in recipes_
    std::unordered_map<ItemId, int> fuelBurnTimes_; // item → burn ticks
};
