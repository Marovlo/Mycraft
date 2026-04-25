#pragma once

#include "renderer/ui_renderer.h"
#include "renderer/block_model.h"
#include "renderer/texture_atlas.h"
#include "player/inventory.h"
#include "crafting/recipe.h"
#include <vector>

// ContainerScreen: abstract base class for all container GUIs (inventory,
// crafting table, furnace, chest, etc.). Provides shared infrastructure:
//   - Slot rendering (drawSlot) with item icons, count, durability
//   - Mouse click interaction (left=swap, right=split/place-1)
//   - Cursor item management (drag & drop)
//   - Panel frame drawing (background, border, dim overlay)
//   - Crafting grid support (optional, configurable size)
//   - Hit-testing across inventory + custom slots
//
// Subclasses only need to implement computeLayout() and getCraftGridSize()
// to define their specific panel geometry and crafting grid dimensions.
class ContainerScreen {
public:
    virtual ~ContainerScreen() = default;

    void init(UIRenderer* ui, BlockModelRenderer* blockModel,
              TextureAtlas* atlas, VulkanEngine* engine);

    virtual void open();
    virtual void close(Inventory& inventory);
    bool isOpen() const { return open_; }

    void handleInput(Inventory& inventory, float screenW, float screenH,
                     double mouseX, double mouseY,
                     bool leftClick, bool rightClick, bool rightHeld = false,
                     bool shiftHeld = false, bool sortPressed = false);

    virtual void draw(float screenW, float screenH, const Inventory& inventory);

    const ItemStack& getCursorItem() const { return cursorItem_; }

protected:
    UIRenderer* ui_ = nullptr;
    BlockModelRenderer* blockModel_ = nullptr;
    TextureAtlas* atlas_ = nullptr;
    VulkanEngine* engine_ = nullptr;

    bool open_ = false;
    ItemStack cursorItem_;

    // Crafting grid (subclass sets size via getCraftGridSize; 0 = no crafting)
    std::vector<ItemStack> craftGrid_;
    ItemStack craftOutput_;

    double mouseX_ = 0, mouseY_ = 0;

    // --- Layout constants (base/unscaled pixels) ---
    static constexpr float SLOT_SIZE   = 18.0f;
    static constexpr float SLOT_GAP    = 2.0f;
    static constexpr float BORDER      = 1.0f;
    static constexpr float ICON_PAD    = 1.0f;
    static constexpr float SECTION_GAP = 6.0f;
    static constexpr float PANEL_PAD   = 8.0f;

    static int getGuiScale(float screenH);

    // Slot types for hit-testing
    enum class SlotType { None, Hotbar, Main, CraftInput, CraftOutput, Container };
    struct SlotHit { SlotType type = SlotType::None; int index = -1; };

    // Right-click drag: track last slot placed into during drag gesture
    SlotHit lastRightDragSlot_;

    struct PanelLayout {
        float panelX, panelY, panelW, panelH;
        float hotbarY, mainY;
        float craftX, craftY;
        float outputX, outputY;
        float containerX, containerY;  // Top-left of container slots (chest, etc.)
        int scale;
    };
    struct SlotRect { float x, y, w, h; };

    // --- Subclass customization points ---

    // Return crafting grid dimensions: {width, height}. E.g. {2,2} for inventory, {3,3} for crafting table.
    // Return {0,0} for screens with no crafting (chest, furnace).
    virtual std::pair<int,int> getCraftGridDims() const = 0;

    // Return number of container slots (e.g. 27 for chest). Default 0 = no container.
    virtual int getContainerSlotCount() const { return 0; }

    // Container slot dimensions for layout: {cols, rows}. Default 9×3 for chest.
    virtual std::pair<int,int> getContainerGridDims() const { return {9, 3}; }

    // Access container slot by index. Subclass must override if getContainerSlotCount() > 0.
    virtual ItemStack* getContainerSlot(int index) { return nullptr; }
    virtual const ItemStack* getContainerSlot(int index) const { return nullptr; }

    // Custom container slot rect (for non-grid layouts like furnace).
    // Default: standard grid layout from containerX/containerY.
    virtual SlotRect getContainerSlotRect(const PanelLayout& L, int index) const;

    // Whether a container slot is output-only (can take from, but not place into).
    // Default: false. Furnace overrides for output slot.
    virtual bool isContainerSlotOutputOnly(int index) const { return false; }

    // Draw container-specific slots. Default: standard grid. Subclass can override
    // for custom layouts (e.g. furnace with fire/arrow progress indicators).
    virtual void drawContainerSlots(const PanelLayout& L, const SlotHit& hover);

    // Compute the full panel layout. Subclass defines geometry.
    virtual PanelLayout computeLayout(float screenW, float screenH) const = 0;

    // Get rect for a craft input slot. Default: grid layout from craftX/craftY.
    virtual SlotRect getCraftSlotRect(const PanelLayout& L, int index) const;

    // Called after crafting grid changes to recompute output.
    void updateCraftOutput();

    // --- Shared implementation (used by draw/handleInput) ---
    SlotRect getSlotRect(const PanelLayout& L, SlotType type, int index) const;
    SlotHit hitTest(const PanelLayout& L, double mx, double my) const;
    void handleSlotClick(Inventory& inventory, SlotHit hit, bool rightClick,
                         bool shiftHeld = false);
    virtual void handleShiftClick(Inventory& inventory, SlotHit hit);
    void sortInventory(Inventory& inventory);
    void sortSlotRange(ItemStack* slots, int count);
    static int itemSortCategory(ItemId id);
    void drawSlot(float x, float y, float size, int scale, const ItemStack& stack,
                  bool highlight = false);
    void drawOutputSlot(const PanelLayout& L, bool highlight);
    void drawArrow(const PanelLayout& L);
    void drawCursorItem(float slotSize, float scale);
    void returnCraftItems(Inventory& inventory);
};
