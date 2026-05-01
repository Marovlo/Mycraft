#include "block_interaction.h"
#include "core/tick_clock.h"
#include "core/debug.h"
#include "entity/entity_manager.h"
#include "audio/sound_engine.h"
#include "audio/block_sound_map.h"

void BlockInteraction::reset() {
    state_ = {};
}

float BlockInteraction::computeBreakDeltaPerTick(const BlockProperties& block,
                                                 const ItemStack& heldTool) {
    if (!block.isBreakable()) return 0.0f;
    if (block.hardness == 0.0f) return 1.0f; // Instant break (e.g. flowers).

    const auto& reg = ItemRegistry::instance();
    const float baseHardness = block.hardness;

    bool toolMatches = false;
    float toolSpeed  = 1.0f;
    int   toolLevel  = 0;
    if (!heldTool.isEmpty()) {
        const auto& itemProps = reg.get(heldTool.id);
        if (itemProps.toolType != ToolType::None &&
            itemProps.toolType == block.requiredToolType) {
            toolMatches = true;
            toolSpeed   = itemProps.miningSpeed;
            toolLevel   = itemProps.miningLevel;
        }
    }

    bool canHarvest = (block.requiredToolType == ToolType::None) ||
                      (toolMatches && toolLevel >= block.requiredMiningLevel);

    float damagePerTick;
    if (canHarvest) {
        damagePerTick = (toolSpeed / baseHardness) / 30.0f;
    } else {
        damagePerTick = (1.0f / baseHardness) / 100.0f;
    }
    return damagePerTick;
}

bool BlockInteraction::canHarvestDrops(const BlockProperties& block,
                                       const ItemStack& heldTool) {
    if (!block.requireToolForDrops) return true;
    if (heldTool.isEmpty()) return false;

    const auto& itemProps = ItemRegistry::instance().get(heldTool.id);
    if (itemProps.toolType != block.requiredToolType) return false;
    if (itemProps.miningLevel < block.requiredMiningLevel) return false;
    return true;
}

void BlockInteraction::spawnDrops(EntityManager& entityMgr,
                                  const BlockProperties& block,
                                  int bx, int by, int bz) {
    glm::vec3 centre(bx + 0.5f, by + 0.5f, bz + 0.5f);
    for (const auto& d : block.drops) {
        if (d.item == Item::None) continue;
        // Simple RNG for variable drops: random count in [minCount, maxCount].
        // Uses a fast hash of position as seed (deterministic per block).
        uint16_t count;
        if (d.minCount == d.maxCount) {
            count = d.minCount;
        } else {
            uint32_t h = static_cast<uint32_t>(bx * 73856093) ^
                         static_cast<uint32_t>(by * 19349663) ^
                         static_cast<uint32_t>(bz * 83492791);
            count = d.minCount + static_cast<uint16_t>(h % (d.maxCount - d.minCount + 1));
        }
        if (count == 0) continue;
        entityMgr.spawnItem(centre, ItemStack{d.item, count, 0});
    }
}

void BlockInteraction::tick(World& world, Player& player, Inventory& inventory,
                            EntityManager& entityMgr,
                            bool leftMouseHeld, float reach) {
    if (!leftMouseHeld) {
        reset();
        return;
    }

    RayHit hit = raycastWorld(world, player.getEyePosition(),
                              player.getForward(), reach);
    if (!hit.hit) {
        reset();
        return;
    }

    BlockId targetId = world.getBlock(hit.blockX, hit.blockY, hit.blockZ);
    const auto& block = BlockRegistry::instance().get(targetId);
    if (!block.isBreakable()) {
        reset();
        return;
    }

    const ItemStack& held = inventory.getHeldItem();

    bool sameTarget = state_.active &&
                      state_.blockX == hit.blockX &&
                      state_.blockY == hit.blockY &&
                      state_.blockZ == hit.blockZ &&
                      state_.blockId == targetId &&
                      state_.toolItemId == held.id;
    if (!sameTarget) {
        state_.active     = true;
        state_.blockX     = hit.blockX;
        state_.blockY     = hit.blockY;
        state_.blockZ     = hit.blockZ;
        state_.blockId    = targetId;
        state_.toolItemId = held.id;
        state_.progress   = 0.0f;
        VLOG(DebugCat::Mining, "start block=%u at (%d,%d,%d) tool=%u",
             targetId, hit.blockX, hit.blockY, hit.blockZ, held.id);
    }

    float delta = computeBreakDeltaPerTick(block, held);
    if (delta <= 0.0f) {
        state_.active = false;
        return;
    }
    state_.progress += delta;

    if (state_.progress >= 1.0f) {
        // 1) Emit drops (if allowed) BEFORE removing the block, so coordinate
        //    reasoning (centre = block centre) is unambiguous.
        bool harvested = canHarvestDrops(block, held);
        if (harvested) {
            spawnDrops(entityMgr, block, state_.blockX, state_.blockY, state_.blockZ);
        }
        VLOG(DebugCat::Mining, "broke block=%u at (%d,%d,%d) harvested=%d",
             state_.blockId, state_.blockX, state_.blockY, state_.blockZ, harvested ? 1 : 0);

        // 2) Notify listeners (e.g. Game drops chest contents)
        if (onBlockBroken) {
            onBlockBroken(state_.blockId, state_.blockX, state_.blockY, state_.blockZ, entityMgr);
        }

        // 3) 播放方块破坏音效（在移除方块之前，此时还能获取方块类型）
        {
            SoundMaterial mat = BlockSoundMap::instance().getMaterial(state_.blockId);
            glm::vec3 blockCenter(state_.blockX + 0.5f, state_.blockY + 0.5f, state_.blockZ + 0.5f);
            getSoundEngine().playBlockBreak(mat, blockCenter);
        }

        // 4) Remove the block. World marks neighbors dirty on its own.
        world.setBlock(state_.blockX, state_.blockY, state_.blockZ, Block::Air);

        // 4) Cost durability on the currently-held tool.
        ItemStack& slot = inventory.getHeldItem();
        if (!slot.isEmpty()) {
            const auto& itemProps = ItemRegistry::instance().get(slot.id);
            if (itemProps.durability > 0) {
                slot.useDurability(1, itemProps.durability);
                // 通知 Game 层标记物品栏脏，同步工具耐久变化给服务器
                if (onInventoryChanged) onInventoryChanged();
            }
        }

        reset();
    }
}
