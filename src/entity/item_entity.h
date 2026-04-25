#pragma once

#include "entity.h"
#include "core/item.h"

// A dropped item floating in the world. Players walking close enough pick it
// up (AABB overlap + small magnetic attract radius). Lifetime is 5 minutes
// unless picked up, after which the entity self-destructs (MC behavior).
class ItemEntity : public Entity {
public:
    ItemStack stack;

    int pickupDelayTicks = 20;      // ~1 s before this drop can be picked up
    int lifetimeTicks    = 6000;    // 5 min = 6000 ticks @ 20 TPS

    // Purely visual:
    float visualYaw   = 0.0f;       // spin angle (radians)
    float visualPhase = 0.0f;       // bob phase offset so a pile of items doesn't beat in sync

    ItemEntity() {
        // 0.25-unit cube matches MC's visual item size.
        halfExtents = glm::vec3(0.125f);
    }

    void tick(World& world, EntityManager& mgr,
              Player& player, Inventory& inventory) override;
    EntityKind kind() const override { return EntityKind::Item; }

    void captureInterpState() override {
        Entity::captureInterpState();
        prevVisualYaw = visualYaw;
    }
};
