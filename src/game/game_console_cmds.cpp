// game_console_cmds.cpp — 控制台命令注册
// 从 Game 类中注册所有调试命令到 GameConsole

#include "game.h"
#include "world/terrain_generator.h"
#include "entity/mob_entity.h"
#include "entity/entity_manager.h"
#include <cmath>
#include <sstream>
#include <algorithm>
#include <iomanip>

void Game::registerConsoleCommands() {
    // /tp x y z — 传送到指定坐标
    console_.registerCommand("tp", "<x> <y> <z> - Teleport to position",
        [this](const std::vector<std::string>& args) -> std::string {
            if (args.size() < 3) return "Usage: /tp <x> <y> <z>";
            try {
                float x = std::stof(args[0]);
                float y = std::stof(args[1]);
                float z = std::stof(args[2]);
                player_.position = glm::vec3(x, y, z);
                player_.velocity = glm::vec3(0);
                player_.fallStartY = player_.position.y;
                player_.wasFalling = false;
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(1);
                oss << "Teleported to " << x << " " << y << " " << z;
                return oss.str();
            } catch (...) {
                return "Invalid coordinates";
            }
        });

    // /tp surface — 传送到当前位置地表
    // /tp spawn — 传送到出生点
    console_.registerCommand("tps", "- Teleport to surface",
        [this](const std::vector<std::string>&) -> std::string {
            auto* gen = dynamic_cast<OverworldGenerator*>(terrainGen_.get());
            int px = static_cast<int>(std::floor(player_.position.x));
            int pz = static_cast<int>(std::floor(player_.position.z));
            int surfY = gen ? gen->getTerrainHeight(px, pz) : 80;
            surfY = std::max(surfY, SEA_LEVEL) + 1;
            player_.position.y = static_cast<float>(surfY);
            player_.velocity = glm::vec3(0);
            player_.fallStartY = player_.position.y;
            player_.wasFalling = false;
            return "Teleported to surface Y=" + std::to_string(surfY);
        });

    console_.registerCommand("spawn", "- Teleport to spawn point",
        [this](const std::vector<std::string>&) -> std::string {
            player_.position = player_.spawnPoint;
            player_.velocity = glm::vec3(0);
            player_.fallStartY = player_.position.y;
            player_.wasFalling = false;
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1);
            oss << "Teleported to spawn (" << player_.spawnPoint.x
                << " " << player_.spawnPoint.y << " " << player_.spawnPoint.z << ")";
            return oss.str();
        });

    // /locate — 定位群系
    console_.registerCommand("locate", "- Find nearby biomes",
        [this](const std::vector<std::string>&) -> std::string {
            auto* gen = dynamic_cast<OverworldGenerator*>(terrainGen_.get());
            if (!gen) return "No overworld generator";
            int cx = static_cast<int>(std::floor(player_.position.x));
            int cz = static_cast<int>(std::floor(player_.position.z));
            const char* biomeNames[] = {"Plains", "Forest", "Desert", "Snowy"};
            std::string result;
            for (int biome = 0; biome < 4; ++biome) {
                bool found = false;
                for (int r = 0; r < 500 && !found; r += 16) {
                    for (int dx = -r; dx <= r && !found; dx += 16) {
                        for (int dz = -r; dz <= r && !found; dz += 16) {
                            if (std::abs(dx) != r && std::abs(dz) != r) continue;
                            auto b = gen->getBiome(cx + dx, cz + dz);
                            if (static_cast<int>(b) == biome) {
                                result += std::string(biomeNames[biome]) + ": "
                                    + std::to_string(cx + dx) + " " + std::to_string(cz + dz) + "\n";
                                found = true;
                            }
                        }
                    }
                }
                if (!found) {
                    result += std::string(biomeNames[biome]) + ": not found\n";
                }
            }
            // 逐行输出
            std::istringstream iss(result);
            std::string line;
            while (std::getline(iss, line)) {
                if (!line.empty()) {
                    console_.print(line, glm::vec4(0.6f, 1.0f, 0.6f, 1.0f));
                }
            }
            return "";
        });

    // /summon <type> [count] — 在玩家前方生成生物
    console_.registerCommand("summon", "<type> [count] - Spawn entity (pig/cow/sheep/chicken/zombie/skeleton/spider/creeper)",
        [this](const std::vector<std::string>& args) -> std::string {
            if (args.empty()) return "Usage: /summon <type> [count]";

            std::string typeName = args[0];
            std::transform(typeName.begin(), typeName.end(), typeName.begin(), ::tolower);

            MobType type;
            if (typeName == "pig") type = MobType::Pig;
            else if (typeName == "cow") type = MobType::Cow;
            else if (typeName == "sheep") type = MobType::Sheep;
            else if (typeName == "chicken") type = MobType::Chicken;
            else if (typeName == "zombie") type = MobType::Zombie;
            else if (typeName == "skeleton") type = MobType::Skeleton;
            else if (typeName == "spider") type = MobType::Spider;
            else if (typeName == "creeper") type = MobType::Creeper;
            else return "Unknown mob type: " + typeName;

            int count = 1;
            if (args.size() >= 2) {
                try { count = std::stoi(args[1]); } catch (...) { count = 1; }
                count = std::clamp(count, 1, 20);
            }

            // 在玩家前方 3 格处生成
            glm::vec3 fwd = player_.getForward();
            fwd.y = 0;
            if (glm::length(fwd) > 0.01f) fwd = glm::normalize(fwd);
            else fwd = glm::vec3(0, 0, -1);

            int spawned = 0;
            for (int i = 0; i < count; i++) {
                glm::vec3 spawnPos = player_.position + fwd * (3.0f + i * 1.5f);
                // 找地面
                int sx = static_cast<int>(std::floor(spawnPos.x));
                int sz = static_cast<int>(std::floor(spawnPos.z));
                int sy = static_cast<int>(std::floor(spawnPos.y));
                // 向下搜索地面
                for (int y = sy + 5; y > sy - 10; y--) {
                    if (BlockRegistry::instance().isSolid(world_.getBlock(sx, y - 1, sz)) &&
                        !BlockRegistry::instance().isSolid(world_.getBlock(sx, y, sz))) {
                        sy = y;
                        break;
                    }
                }

                auto mob = std::make_unique<MobEntity>(type);
                mob->position = glm::vec3(sx + 0.5f, sy + mob->mobHeight * 0.5f, sz + 0.5f);
                mob->prevPosition = mob->position;
                mob->bodyYaw = player_.yaw + 3.14159f;  // 面向玩家
                mob->prevBodyYaw = mob->bodyYaw;
                entityManager_.addEntity(std::move(mob));
                spawned++;
            }

            return "Spawned " + std::to_string(spawned) + " " + typeName;
        });

    // /kill — 杀死附近所有生物
    console_.registerCommand("kill", "[type] - Kill all mobs or specific type",
        [this](const std::vector<std::string>& args) -> std::string {
            std::string filter;
            if (!args.empty()) {
                filter = args[0];
                std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
            }

            int killed = 0;
            for (auto& e : entityManager_.entities()) {
                if (!e || !e->alive || e->kind() != EntityKind::Mob) continue;
                auto& mob = static_cast<MobEntity&>(*e);
                if (!filter.empty()) {
                    const auto& props = MobRegistry::instance().get(mob.mobType);
                    std::string name = props.name;
                    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                    if (name != filter) continue;
                }
                mob.hp = 0;
                mob.alive = false;
                killed++;
            }
            return "Killed " + std::to_string(killed) + " mobs";
        });

    // /pos — 显示当前位置
    console_.registerCommand("pos", "- Show current position",
        [this](const std::vector<std::string>&) -> std::string {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2);
            oss << "Position: " << player_.position.x << " "
                << player_.position.y << " " << player_.position.z;
            console_.print(oss.str(), glm::vec4(0.6f, 1.0f, 0.6f, 1.0f));
            oss.str("");
            oss << "Yaw: " << player_.yaw << " Pitch: " << player_.pitch;
            console_.print(oss.str(), glm::vec4(0.6f, 1.0f, 0.6f, 1.0f));
            oss.str("");
            oss << "Chunk: " << playerChunkX_ << " " << playerChunkZ_;
            return oss.str();
        });

    // /time <set|query> [value] — 设置/查询时间
    console_.registerCommand("time", "<set|query> [ticks] - Set or query world time",
        [this](const std::vector<std::string>& args) -> std::string {
            if (args.empty()) return "Usage: /time <set|query> [ticks]";
            std::string sub = args[0];
            std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);
            if (sub == "query") {
                uint32_t t = dayNightCycle_.getTime();
                bool night = dayNightCycle_.isNight();
                return "Time: " + std::to_string(t) + (night ? " (night)" : " (day)");
            } else if (sub == "set" && args.size() >= 2) {
                try {
                    uint32_t t = static_cast<uint32_t>(std::stoul(args[1]));
                    dayNightCycle_.setTime(t % 24000);
                    return "Time set to " + std::to_string(t % 24000);
                } catch (...) {
                    return "Invalid time value";
                }
            }
            return "Usage: /time <set|query> [ticks]";
        });

    // /heal — 恢复满血满饥饿
    console_.registerCommand("heal", "- Restore full health and hunger",
        [this](const std::vector<std::string>&) -> std::string {
            player_.hp = player_.maxHp;
            player_.hunger = player_.maxHunger;
            player_.air = player_.maxAir;
            return "Healed to full";
        });

    // /give <item> [count] — 给予物品
    console_.registerCommand("give", "<item_name> [count] - Give item to player",
        [this](const std::vector<std::string>& args) -> std::string {
            if (args.empty()) return "Usage: /give <item_name> [count]";
            std::string name = args[0];
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);

            int count = 1;
            if (args.size() >= 2) {
                try { count = std::stoi(args[1]); } catch (...) { count = 1; }
                count = std::clamp(count, 1, 64);
            }

            // 搜索物品注册表
            const auto& reg = ItemRegistry::instance();
            for (uint16_t id = 1; id < 256; id++) {
                const auto& props = reg.get(id);
                if (props.displayName.empty()) continue;
                std::string dn = props.displayName;
                std::transform(dn.begin(), dn.end(), dn.begin(), ::tolower);
                // 支持部分匹配
                if (dn.find(name) != std::string::npos) {
                    ItemStack stack{id, static_cast<uint16_t>(count), 0};
                    inventory_.addItem(stack);
                    return "Gave " + std::to_string(count) + " " + props.displayName;
                }
            }
            return "Item not found: " + name;
        });

    // /mobs — 列出当前生物数量
    console_.registerCommand("mobs", "- List mob counts",
        [this](const std::vector<std::string>&) -> std::string {
            int counts[static_cast<int>(MobType::COUNT)] = {};
            int total = 0;
            for (const auto& e : entityManager_.entities()) {
                if (!e || !e->alive || e->kind() != EntityKind::Mob) continue;
                const auto& mob = static_cast<const MobEntity&>(*e);
                counts[static_cast<int>(mob.mobType)]++;
                total++;
            }
            const char* names[] = {"Pig", "Cow", "Sheep", "Chicken", "Zombie", "Skeleton", "Spider", "Creeper"};
            for (int i = 0; i < static_cast<int>(MobType::COUNT); i++) {
                if (counts[i] > 0) {
                    console_.print(std::string(names[i]) + ": " + std::to_string(counts[i]),
                                   glm::vec4(0.6f, 1.0f, 0.6f, 1.0f));
                }
            }
            return "Total: " + std::to_string(total) + " mobs";
        });

    // /seed — 显示世界种子
    console_.registerCommand("seed", "- Show world seed",
        [this](const std::vector<std::string>&) -> std::string {
            return "Seed: " + std::to_string(worldSeed_);
        });

    // /save — 手动保存
    console_.registerCommand("save", "- Save world",
        [this](const std::vector<std::string>&) -> std::string {
            saveAll();
            return "World saved";
        });
}
