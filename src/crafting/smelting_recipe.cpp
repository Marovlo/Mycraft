#include "smelting_recipe.h"

SmeltingRegistry& SmeltingRegistry::instance() {
    static SmeltingRegistry inst;
    return inst;
}

void SmeltingRegistry::registerRecipe(ItemId input, ItemStack output, float xp,
                                      int smeltTime) {
    int idx = static_cast<int>(recipes_.size());
    recipes_.push_back({input, output, xp, smeltTime});
    recipeIndex_[input] = idx;
}

void SmeltingRegistry::registerFuel(ItemId item, int burnTimeTicks) {
    fuelBurnTimes_[item] = burnTimeTicks;
}

const SmeltingRecipe* SmeltingRegistry::findRecipe(ItemId input) const {
    auto it = recipeIndex_.find(input);
    if (it == recipeIndex_.end()) return nullptr;
    return &recipes_[it->second];
}

int SmeltingRegistry::getFuelBurnTime(ItemId item) const {
    auto it = fuelBurnTimes_.find(item);
    return (it != fuelBurnTimes_.end()) ? it->second : 0;
}

bool SmeltingRegistry::isFuel(ItemId item) const {
    return fuelBurnTimes_.count(item) > 0;
}

void SmeltingRegistry::registerDefaults() {
    // ========== 冶炼配方（MC原版） ==========
    // 矿石原料 → 锭
    registerRecipe(Item::RawIron,   {Item::IronIngot,   1, 0}, 0.7f);
    registerRecipe(Item::RawGold,   {Item::GoldIngot,   1, 0}, 1.0f);
    registerRecipe(Item::RawCopper, {Item::CopperIngot, 1, 0}, 0.7f);

    // 矿石方块 → 对应产物（精炼矿石方块，MC原版支持）
    registerRecipe(Item::IronOre,   {Item::IronIngot,   1, 0}, 0.7f);
    registerRecipe(Item::GoldOre,   {Item::GoldIngot,   1, 0}, 1.0f);
    registerRecipe(Item::CopperOre, {Item::CopperIngot, 1, 0}, 0.7f);

    // 圆石 → 石头
    registerRecipe(Item::Cobblestone, {Item::Stone, 1, 0}, 0.1f);

    // 沙子 → 玻璃（预留，当前没有玻璃方块）
    // registerRecipe(Item::Sand, {Item::Glass, 1, 0}, 0.1f);

    // 原木 → 木炭（MC原版：木炭和煤功能相同，这里产出煤代替）
    registerRecipe(Item::OakLog, {Item::Coal, 1, 0}, 0.15f);

    // 食物冶炼（预留）
    // registerRecipe(Item::RawBeef, {Item::CookedBeef, 1, 0}, 0.35f);

    // ========== 燃料（MC原版燃烧时间，单位tick，20tick=1秒） ==========
    // 木质物品
    registerFuel(Item::OakPlanks,     300);    // 15秒（1.5个物品）
    registerFuel(Item::Stick,         100);    // 5秒（0.5个物品）
    registerFuel(Item::OakLog,        300);    // 15秒
    registerFuel(Item::CraftingTable, 300);    // 15秒
    registerFuel(Item::Chest,         300);    // 15秒
    registerFuel(Item::WoodenPickaxe, 200);    // 10秒
    registerFuel(Item::WoodenAxe,     200);    // 10秒
    registerFuel(Item::WoodenShovel,  200);    // 10秒
    registerFuel(Item::WoodenSword,   200);    // 10秒
    registerFuel(Item::WoodenHoe,     200);    // 10秒

    // 煤炭（MC原版：1600 ticks = 80秒 = 冶炼8个物品）
    registerFuel(Item::Coal, 1600);

    // 木炭同煤（这里用煤代替木炭，所以不需要额外注册）
}
