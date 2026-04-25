#include "furnace_manager.h"
#include "crafting/smelting_recipe.h"
#include "core/debug.h"

FurnaceManager::FurnaceData& FurnaceManager::getOrCreate(int x, int y, int z) {
    uint64_t key = packKey(x, y, z);
    auto it = furnaces_.find(key);
    if (it != furnaces_.end()) return it->second;

    auto& data = furnaces_[key];
    data.inputSlot.clear();
    data.fuelSlot.clear();
    data.outputSlot.clear();
    data.smeltProgress = 0;
    data.smeltTotalTicks = 200;
    data.fuelBurnRemaining = 0;
    data.fuelBurnTotal = 0;
    VLOG(DebugCat::Save, "Created furnace at (%d,%d,%d)", x, y, z);
    return data;
}

FurnaceManager::FurnaceData* FurnaceManager::get(int x, int y, int z) {
    uint64_t key = packKey(x, y, z);
    auto it = furnaces_.find(key);
    return (it != furnaces_.end()) ? &it->second : nullptr;
}

const FurnaceManager::FurnaceData* FurnaceManager::get(int x, int y, int z) const {
    uint64_t key = packKey(x, y, z);
    auto it = furnaces_.find(key);
    return (it != furnaces_.end()) ? &it->second : nullptr;
}

FurnaceManager::FurnaceData FurnaceManager::remove(int x, int y, int z) {
    uint64_t key = packKey(x, y, z);
    auto it = furnaces_.find(key);
    if (it == furnaces_.end()) {
        FurnaceData empty;
        return empty;
    }
    FurnaceData contents = it->second;
    furnaces_.erase(it);
    VLOG(DebugCat::Save, "Removed furnace at (%d,%d,%d)", x, y, z);
    return contents;
}

bool FurnaceManager::has(int x, int y, int z) const {
    return furnaces_.count(packKey(x, y, z)) > 0;
}

// ============================================================
// Tick — MC原版熔炉逻辑
// ============================================================

void FurnaceManager::tick() {
    for (auto& [key, data] : furnaces_) {
        tickFurnace(data);
    }
}

void FurnaceManager::tickFurnace(FurnaceData& data) {
    const auto& registry = SmeltingRegistry::instance();
    bool wasBurning = data.isBurning();

    // 1) 燃料燃烧倒计时
    if (data.fuelBurnRemaining > 0) {
        --data.fuelBurnRemaining;
    }

    // 2) 检查是否可以冶炼当前输入
    const SmeltingRecipe* recipe = nullptr;
    bool canSmelt = false;

    if (!data.inputSlot.isEmpty()) {
        recipe = registry.findRecipe(data.inputSlot.id);
        if (recipe) {
            // 检查输出槽是否能容纳产物
            if (data.outputSlot.isEmpty()) {
                canSmelt = true;
            } else if (data.outputSlot.id == recipe->output.id) {
                const auto& outProps = ItemRegistry::instance().get(recipe->output.id);
                if (data.outputSlot.count + recipe->output.count <= outProps.maxStackSize) {
                    canSmelt = true;
                }
            }
        }
    }

    // 3) 如果可以冶炼但燃料耗尽，尝试消耗新燃料
    if (canSmelt && !data.isBurning()) {
        if (!data.fuelSlot.isEmpty()) {
            int burnTime = registry.getFuelBurnTime(data.fuelSlot.id);
            if (burnTime > 0) {
                data.fuelBurnTotal = burnTime;
                data.fuelBurnRemaining = burnTime;
                // 消耗一个燃料
                data.fuelSlot.count -= 1;
                if (data.fuelSlot.count == 0) data.fuelSlot.clear();
            }
        }
    }

    // 4) 冶炼进度推进
    if (data.isBurning() && canSmelt && recipe) {
        data.smeltTotalTicks = recipe->smeltTimeTicks;
        ++data.smeltProgress;

        // 冶炼完成
        if (data.smeltProgress >= data.smeltTotalTicks) {
            data.smeltProgress = 0;

            // 消耗输入
            data.inputSlot.count -= 1;
            if (data.inputSlot.count == 0) data.inputSlot.clear();

            // 产出到输出槽
            if (data.outputSlot.isEmpty()) {
                data.outputSlot = recipe->output;
            } else {
                data.outputSlot.count += recipe->output.count;
            }
        }
    } else {
        // 不能冶炼时，进度逐渐回退（MC原版行为：每tick减2）
        if (data.smeltProgress > 0) {
            data.smeltProgress = std::max(0, data.smeltProgress - 2);
        }
    }
}

// ============================================================
// 序列化
// ============================================================

void FurnaceManager::serialize(BinaryWriter& w) const {
    w.writeU32(static_cast<uint32_t>(furnaces_.size()));

    for (const auto& [key, data] : furnaces_) {
        int32_t x = static_cast<int32_t>(static_cast<uint32_t>(key & 0xFFFFFFFF));
        int32_t y = static_cast<int32_t>(static_cast<uint16_t>((key >> 32) & 0xFFFF));
        int32_t z = static_cast<int32_t>(static_cast<int16_t>((key >> 48) & 0xFFFF));

        w.writeI32(x);
        w.writeI32(y);
        w.writeI32(z);

        // 3个槽位
        auto writeSlot = [&](const ItemStack& s) {
            w.writeU16(s.id);
            w.writeU16(s.count);
            w.writeU16(s.durability);
        };
        writeSlot(data.inputSlot);
        writeSlot(data.fuelSlot);
        writeSlot(data.outputSlot);

        // 冶炼状态
        w.writeI32(data.smeltProgress);
        w.writeI32(data.smeltTotalTicks);
        w.writeI32(data.fuelBurnRemaining);
        w.writeI32(data.fuelBurnTotal);
    }
}

void FurnaceManager::deserialize(BinaryReader& r) {
    furnaces_.clear();

    uint32_t count = r.readU32();
    for (uint32_t c = 0; c < count && r.isValid(); ++c) {
        int32_t x = r.readI32();
        int32_t y = r.readI32();
        int32_t z = r.readI32();

        uint64_t key = packKey(x, y, z);
        FurnaceData& data = furnaces_[key];

        auto readSlot = [&]() -> ItemStack {
            ItemStack s;
            s.id = r.readU16();
            s.count = r.readU16();
            s.durability = r.readU16();
            return s;
        };
        data.inputSlot = readSlot();
        data.fuelSlot = readSlot();
        data.outputSlot = readSlot();

        data.smeltProgress = r.readI32();
        data.smeltTotalTicks = r.readI32();
        data.fuelBurnRemaining = r.readI32();
        data.fuelBurnTotal = r.readI32();
    }

    VLOG(DebugCat::Save, "Loaded %u furnace inventories", count);
}
