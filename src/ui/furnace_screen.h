#pragma once

#include "container_screen.h"
#include "world/furnace_manager.h"

// FurnaceScreen: MC原版熔炉GUI
// 布局（从上到下）：
//   [输入槽]          [箭头→]  [输出槽]
//   [火焰↑]
//   [燃料槽]
//   分隔线
//   3行主背包
//   分隔线
//   1行快捷栏
//
// 3个Container槽位：0=输入, 1=燃料, 2=输出
// 输出槽(index=2)为只读（只能取出，不能放入）
class FurnaceScreen : public ContainerScreen {
public:
    void setFurnaceData(FurnaceManager::FurnaceData* data) { furnaceData_ = data; }

    void open() override;
    void close(Inventory& inventory) override;

    // 覆盖基类draw：自定义绘制火焰/箭头进度
    void draw(float screenW, float screenH, const Inventory& inventory) override;

protected:
    std::pair<int,int> getCraftGridDims() const override { return {0, 0}; }
    PanelLayout computeLayout(float screenW, float screenH) const override;

    // 熔炉有3个特殊槽位（输入、燃料、输出），通过Container槽位系统管理
    int getContainerSlotCount() const override { return 3; }
    std::pair<int,int> getContainerGridDims() const override { return {1, 3}; }

    ItemStack* getContainerSlot(int index) override;
    const ItemStack* getContainerSlot(int index) const override;

    // 自定义Container槽位位置（非标准网格布局）
    SlotRect getContainerSlotRect(const PanelLayout& L, int index) const override;

    // 输出槽(index=2)只能取出，不能放入
    bool isContainerSlotOutputOnly(int index) const override { return index == SLOT_OUTPUT; }

    // 自定义Container槽位绘制（火焰+箭头+3个槽位）
    void drawContainerSlots(const PanelLayout& L, const SlotHit& hover) override;

    // 熔炉特有的Shift+Click逻辑
    void handleShiftClick(Inventory& inventory, SlotHit hit) override;

private:
    FurnaceManager::FurnaceData* furnaceData_ = nullptr;

    // 熔炉槽位索引常量
    static constexpr int SLOT_INPUT  = 0;
    static constexpr int SLOT_FUEL   = 1;
    static constexpr int SLOT_OUTPUT = 2;

    // 计算熔炉区域内各元素的绝对坐标
    struct FurnaceLayout {
        float inputX, inputY;
        float fuelX, fuelY;
        float outputX, outputY;
        float flameX, flameY, flameSize;
        float arrowX, arrowY, arrowW, arrowH;
        float slot;  // 槽位尺寸
    };
    FurnaceLayout computeFurnaceLayout(const PanelLayout& L) const;

    // 绘制辅助：火焰进度和箭头进度
    void drawFlameProgress(float x, float y, float size, float progress);
    void drawArrowProgress(float x, float y, float w, float h, float progress);

    // 绘制金色边框输出槽
    void drawOutputSlotAt(float x, float y, float size, int scale,
                          const ItemStack& stack, bool highlight);
};
