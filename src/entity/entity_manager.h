#pragma once

#include "entity.h"
#include "item_entity.h"
#include "arrow_entity.h"
#include "xp_orb_entity.h"
#include <memory>
#include <vector>

class World;
class Player;
class Inventory;

// Owns and ticks all non-player, non-block entities. Keeps them in a simple
// flat vector; per-chunk bucketing can be added later when entity counts grow.
class EntityManager {
public:
    // Per-tick update: physics + AI + lifetime. Dead entities are swept out at
    // the end so borrowed references within the loop stay valid.
    void tick(World& world, Player& player, Inventory& inventory);

    // Spawn an ItemEntity at worldPos with the given stack and optional initial
    // velocity. Velocity default gives a tiny random upward bounce like MC.
    void spawnItem(const glm::vec3& worldPos, const ItemStack& stack,
                   const glm::vec3& initialVel = glm::vec3(0.0f, 4.0f, 0.0f));

    // 生成箭矢实体：从 from 位置沿 dir 方向以 speed 速度射出
    void spawnArrow(const glm::vec3& from, const glm::vec3& dir,
                    float speed, int damage, bool fromPlayer = false);

    // 生成经验球：在 worldPos 位置生成包含 totalXP 经验值的经验球（自动拆分）
    void spawnXPOrbs(const glm::vec3& worldPos, int totalXP);

    const std::vector<std::unique_ptr<Entity>>& entities() const { return entities_; }
    size_t count() const { return entities_.size(); }
    void clear() { entities_.clear(); }

    // Direct insertion (used by save/load to restore persisted entities)
    void addEntity(std::unique_ptr<Entity> e) { entities_.push_back(std::move(e)); }

private:
    std::vector<std::unique_ptr<Entity>> entities_;
};
