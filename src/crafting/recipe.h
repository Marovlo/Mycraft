#pragma once

#include "core/item.h"
#include <vector>
#include <array>
#include <algorithm>

// A crafting recipe. MC has two kinds:
//   shaped:    items must be in exact grid positions (can shift but not rearrange)
//   shapeless: items just need to all be present in any slots
//
// The grid is stored as a 1D array in row-major order, gridWidth × gridHeight.
// Empty cells are Item::None (0).
struct Recipe {
    bool shaped = true;           // false = shapeless
    int  gridWidth  = 0;          // 1..3
    int  gridHeight = 0;          // 1..3
    std::vector<ItemId> grid;     // gridWidth * gridHeight entries

    ItemStack output;             // what you get

    // Helper: grid cell at (col, row).
    ItemId at(int col, int row) const {
        return grid[row * gridWidth + col];
    }
};

// Singleton registry of all recipes. Scanned linearly on each craft attempt —
// fine for <100 recipes; MC itself uses a similar approach with caching.
class RecipeRegistry {
public:
    static RecipeRegistry& instance();

    void registerRecipe(Recipe r) { recipes_.push_back(std::move(r)); }

    // Try to match the given crafting grid (row-major, gridW × gridH) against
    // all registered recipes. Returns nullptr if no match.
    // For shaped recipes the pattern is slid across the grid and optionally
    // mirrored horizontally.
    const Recipe* findMatch(const ItemId* grid, int gridW, int gridH) const;

    // Convenience: try to craft from a flat inventory of items (shapeless-only
    // scan). Used by the simplified auto-craft system (Batch 5 Phase 2).
    // `available` is a bag of ItemId → count. Returns the first matching recipe.
    const Recipe* findShapelessMatch(const std::vector<std::pair<ItemId,int>>& available) const;

    void registerDefaults();

    const std::vector<Recipe>& all() const { return recipes_; }

private:
    RecipeRegistry() = default;
    std::vector<Recipe> recipes_;

    // Internal: check if a shaped recipe matches the grid at a given offset.
    static bool shapedMatchAt(const Recipe& r, const ItemId* grid, int gridW, int gridH,
                              int offX, int offY, bool mirror);
};
