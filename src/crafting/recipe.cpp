#include "recipe.h"
#include "core/block.h"
#include <unordered_map>

RecipeRegistry& RecipeRegistry::instance() {
    static RecipeRegistry reg;
    return reg;
}

// ---- Shaped matching ----

bool RecipeRegistry::shapedMatchAt(const Recipe& r, const ItemId* grid,
                                   int gridW, int gridH,
                                   int offX, int offY, bool mirror) {
    for (int ry = 0; ry < r.gridHeight; ++ry) {
        for (int rx = 0; rx < r.gridWidth; ++rx) {
            int gx = offX + (mirror ? (r.gridWidth - 1 - rx) : rx);
            int gy = offY + ry;
            if (gx < 0 || gx >= gridW || gy < 0 || gy >= gridH) return false;
            ItemId need = r.grid[ry * r.gridWidth + rx];
            ItemId have = grid[gy * gridW + gx];
            if (need != have) return false;
        }
    }
    // All cells outside the recipe footprint must be empty.
    for (int gy = 0; gy < gridH; ++gy) {
        for (int gx = 0; gx < gridW; ++gx) {
            bool insideRecipe = (gx >= offX && gx < offX + r.gridWidth &&
                                 gy >= offY && gy < offY + r.gridHeight);
            if (!insideRecipe && grid[gy * gridW + gx] != Item::None) return false;
        }
    }
    return true;
}

const Recipe* RecipeRegistry::findMatch(const ItemId* grid, int gridW, int gridH) const {
    for (const auto& r : recipes_) {
        if (r.shaped) {
            // Slide the recipe pattern across every valid offset, both normal and mirrored.
            for (int oy = 0; oy <= gridH - r.gridHeight; ++oy) {
                for (int ox = 0; ox <= gridW - r.gridWidth; ++ox) {
                    if (shapedMatchAt(r, grid, gridW, gridH, ox, oy, false)) return &r;
                    if (shapedMatchAt(r, grid, gridW, gridH, ox, oy, true))  return &r;
                }
            }
        } else {
            // Shapeless: count items in grid, compare to recipe.
            std::unordered_map<ItemId, int> gridBag;
            for (int i = 0; i < gridW * gridH; ++i) {
                if (grid[i] != Item::None) gridBag[grid[i]]++;
            }
            std::unordered_map<ItemId, int> recipeBag;
            for (auto id : r.grid) {
                if (id != Item::None) recipeBag[id]++;
            }
            if (gridBag == recipeBag) return &r;
        }
    }
    return nullptr;
}

const Recipe* RecipeRegistry::findShapelessMatch(
        const std::vector<std::pair<ItemId,int>>& available) const {
    for (const auto& r : recipes_) {
        // Build the recipe's required bag.
        std::unordered_map<ItemId, int> need;
        for (auto id : r.grid) {
            if (id != Item::None) need[id]++;
        }
        // Check all requirements are satisfied.
        bool ok = true;
        for (auto& [id, cnt] : need) {
            bool found = false;
            for (auto& [aid, acnt] : available) {
                if (aid == id && acnt >= cnt) { found = true; break; }
            }
            if (!found) { ok = false; break; }
        }
        if (ok) return &r;
    }
    return nullptr;
}

// ---- Default recipes (Phase 2 initial set) ----

void RecipeRegistry::registerDefaults() {
    // Helper to build a shaped recipe from a pattern string.
    // ' ' = empty, characters map to ItemIds via a legend.
    auto shaped = [&](int w, int h, const std::vector<ItemId>& pat, ItemStack out) {
        Recipe r;
        r.shaped = true;
        r.gridWidth = w;
        r.gridHeight = h;
        r.grid = pat;
        r.output = out;
        registerRecipe(std::move(r));
    };
    auto shapeless = [&](const std::vector<ItemId>& pat, ItemStack out) {
        Recipe r;
        r.shaped = false;
        r.gridWidth = 1;
        r.gridHeight = static_cast<int>(pat.size());
        r.grid = pat;
        r.output = out;
        registerRecipe(std::move(r));
    };

    const ItemId N = Item::None;
    const ItemId P = Item::OakPlanks;  // plank (item id for oak_planks block item)
    const ItemId S = Item::Stick;
    const ItemId L = Item::OakLog;
    const ItemId C = Item::Cobblestone;

    // Oak Log → 4 Oak Planks (shapeless, 1 ingredient)
    shapeless({L}, {P, 4, 0});

    // 2 Oak Planks (vertical) → 4 Sticks
    shaped(1, 2, {P, P}, {S, 4, 0});

    // 4 Planks 2×2 → 1 Crafting Table
    shaped(2, 2, {P,P, P,P}, {Item::CraftingTable, 1, 0});

    // --- Wooden tools (3×3 grid) ---
    // Pickaxe: PPP / .S. / .S.
    shaped(3, 3, {P,P,P, N,S,N, N,S,N}, {Item::WoodenPickaxe, 1, 0});
    // Axe: PP. / PS. / .S.
    shaped(3, 3, {P,P,N, P,S,N, N,S,N}, {Item::WoodenAxe, 1, 0});
    // Shovel: .P. / .S. / .S.
    shaped(3, 3, {N,P,N, N,S,N, N,S,N}, {Item::WoodenShovel, 1, 0});
    // Sword: .P. / .P. / .S.
    shaped(3, 3, {N,P,N, N,P,N, N,S,N}, {Item::WoodenSword, 1, 0});
    // Hoe: PP. / .S. / .S.
    shaped(3, 3, {P,P,N, N,S,N, N,S,N}, {Item::WoodenHoe, 1, 0});

    // --- Stone tools (same patterns, cobblestone replaces planks) ---
    shaped(3, 3, {C,C,C, N,S,N, N,S,N}, {Item::StonePickaxe, 1, 0});
    shaped(3, 3, {C,C,N, C,S,N, N,S,N}, {Item::StoneAxe, 1, 0});
    shaped(3, 3, {N,C,N, N,S,N, N,S,N}, {Item::StoneShovel, 1, 0});
    shaped(3, 3, {N,C,N, N,C,N, N,S,N}, {Item::StoneSword, 1, 0});
    shaped(3, 3, {C,C,N, N,S,N, N,S,N}, {Item::StoneHoe, 1, 0});

    // --- Iron tools ---
    const ItemId I = Item::IronIngot;
    shaped(3, 3, {I,I,I, N,S,N, N,S,N}, {Item::IronPickaxe, 1, 0});
    shaped(3, 3, {I,I,N, I,S,N, N,S,N}, {Item::IronAxe, 1, 0});
    shaped(3, 3, {N,I,N, N,S,N, N,S,N}, {Item::IronShovel, 1, 0});
    shaped(3, 3, {N,I,N, N,I,N, N,S,N}, {Item::IronSword, 1, 0});
    shaped(3, 3, {I,I,N, N,S,N, N,S,N}, {Item::IronHoe, 1, 0});

    // --- Gold tools ---
    const ItemId G = Item::GoldIngot;
    shaped(3, 3, {G,G,G, N,S,N, N,S,N}, {Item::GoldPickaxe, 1, 0});
    shaped(3, 3, {G,G,N, G,S,N, N,S,N}, {Item::GoldAxe, 1, 0});
    shaped(3, 3, {N,G,N, N,S,N, N,S,N}, {Item::GoldShovel, 1, 0});
    shaped(3, 3, {N,G,N, N,G,N, N,S,N}, {Item::GoldSword, 1, 0});
    shaped(3, 3, {G,G,N, N,S,N, N,S,N}, {Item::GoldHoe, 1, 0});

    // --- Diamond tools ---
    const ItemId D2 = Item::Diamond;
    shaped(3, 3, {D2,D2,D2, N,S,N, N,S,N}, {Item::DiamondPickaxe, 1, 0});
    shaped(3, 3, {D2,D2,N, D2,S,N, N,S,N}, {Item::DiamondAxe, 1, 0});
    shaped(3, 3, {N,D2,N, N,S,N, N,S,N},   {Item::DiamondShovel, 1, 0});
    shaped(3, 3, {N,D2,N, N,D2,N, N,S,N},  {Item::DiamondSword, 1, 0});
    shaped(3, 3, {D2,D2,N, N,S,N, N,S,N},  {Item::DiamondHoe, 1, 0});

    // --- Furnace: 8 cobblestone ring ---
    shaped(3, 3, {C,C,C, C,N,C, C,C,C}, {Item::Furnace, 1, 0});

    // --- Torch: coal on top of stick ---
    shaped(1, 2, {Item::Coal, S}, {Item::Torch, 4, 0});

    // --- Chest: 8 planks ring ---
    shaped(3, 3, {P,P,P, P,N,P, P,P,P}, {Item::Chest, 1, 0});
}
