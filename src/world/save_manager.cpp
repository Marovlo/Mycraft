#include "save_manager.h"
#include "core/serialization.h"
#include "core/debug.h"
#include "player/player.h"
#include "player/inventory.h"
#include "entity/entity_manager.h"
#include "entity/item_entity.h"
#include "entity/mob_entity.h"
#include "chunk.h"
#include "chunk_serializer.h"
#include "world.h"

#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

// ========== Lifecycle ==========

SaveManager::~SaveManager() {
    closeAllRegions();
}

// ========== Helpers ==========

bool SaveManager::ensureDirectory(const std::string& path) {
    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec;
}

bool SaveManager::fileExists(const std::string& path) {
    return fs::exists(path);
}

// ========== World setup ==========

void SaveManager::setWorld(const std::string& worldName, const std::string& basePath) {
    closeAllRegions();

    worldName_ = worldName;
    worldDir_  = basePath + "/" + worldName;
    regionDir_ = worldDir_ + "/region";

    ensureDirectory(worldDir_);
    ensureDirectory(regionDir_);

    // Crash recovery: clean up any orphaned .tmp files from interrupted writes
    {
        std::error_code ec;
        for (auto& entry : fs::directory_iterator(worldDir_, ec)) {
            if (entry.path().extension() == ".tmp") {
                fs::remove(entry.path(), ec);
                std::cout << "[Save] Removed incomplete file: " << entry.path().filename() << "\n";
            }
        }
    }
}

std::string SaveManager::playerPath() const {
    return worldDir_ + "/players/player.dat";
}

std::string SaveManager::playerPathByName(const std::string& playerName) const {
    return worldDir_ + "/players/" + playerName + ".dat";
}

std::string SaveManager::levelPath() const {
    return worldDir_ + "/level.dat";
}

std::string SaveManager::entitiesPath() const {
    return worldDir_ + "/entities.dat";
}

// ========== Player save/load ==========

bool SaveManager::savePlayerByName(const std::string& playerName,
                                    const Player& player, const Inventory& inventory) {
    // 确保 players/ 目录存在
    ensureDirectory(worldDir_ + "/players");

    std::string path = playerPathByName(playerName);
    std::string tmpPath = path + ".tmp";
    {
        BinaryWriter w(tmpPath);
        if (!w.isValid()) {
            std::cerr << "[Save] Failed to open " << tmpPath << " for writing\n";
            return false;
        }

        w.writeHeader(VCFile::Type::Player, VCFile::VERSION_PLAYER);
        player.serialize(w);
        inventory.serialize(w);
        w.close();
    }

    // Atomic replace: rename tmp → final
    std::error_code ec;
    fs::rename(tmpPath, path, ec);
    if (ec) {
        std::cerr << "[Save] Failed to rename " << tmpPath << " → " << path
                  << ": " << ec.message() << "\n";
        fs::remove(tmpPath, ec);
        return false;
    }

    VLOG(DebugCat::Save, "Player '%s' data saved to %s", playerName.c_str(), path.c_str());
    return true;
}

bool SaveManager::loadPlayerByName(const std::string& playerName,
                                    Player& player, Inventory& inventory) {
    std::string path = playerPathByName(playerName);
    if (!fileExists(path)) return false;

    BinaryReader r(path);
    if (!r.isValid()) return false;

    uint16_t version;
    if (!r.readHeader(VCFile::Type::Player, VCFile::VERSION_PLAYER, version)) {
        std::cerr << "[Save] Invalid player.dat header for '" << playerName << "'\n";
        return false;
    }

    player.deserialize(r);
    inventory.deserialize(r);

    if (!r.isValid()) {
        std::cerr << "[Save] player.dat appears truncated for '" << playerName << "'\n";
        return false;
    }

    VLOG(DebugCat::Save, "Player '%s' data loaded from %s", playerName.c_str(), path.c_str());
    return true;
}

// Legacy shortcuts — delegate to named version with "player"
bool SaveManager::savePlayer(const Player& player, const Inventory& inventory) {
    return savePlayerByName("player", player, inventory);
}

bool SaveManager::loadPlayer(Player& player, Inventory& inventory) {
    return loadPlayerByName("player", player, inventory);
}

// ========== Level data save/load ==========

bool SaveManager::saveLevelData(int64_t seed, uint64_t totalTicks,
                                float spawnX, float spawnY, float spawnZ,
                                const std::string& worldName) {
    std::string tmpPath = levelPath() + ".tmp";
    {
        BinaryWriter w(tmpPath);
        if (!w.isValid()) return false;

        w.writeHeader(VCFile::Type::Level, VCFile::VERSION_LEVEL);
        w.writeString(worldName);
        w.writeI64(seed);
        w.writeU8(0);  // gameMode: 0=survival
        w.writeU64(totalTicks);
        w.writeF32(spawnX);
        w.writeF32(spawnY);
        w.writeF32(spawnZ);
        w.close();
    }

    std::error_code ec;
    fs::rename(tmpPath, levelPath(), ec);
    if (ec) {
        fs::remove(tmpPath, ec);
        return false;
    }

    return true;
}

bool SaveManager::loadLevelData(int64_t& seed, uint64_t& totalTicks,
                                float& spawnX, float& spawnY, float& spawnZ,
                                std::string& worldName) {
    if (!fileExists(levelPath())) return false;

    BinaryReader r(levelPath());
    if (!r.isValid()) return false;

    uint16_t version;
    if (!r.readHeader(VCFile::Type::Level, VCFile::VERSION_LEVEL, version)) {
        return false;
    }

    worldName  = r.readString();
    seed       = r.readI64();
    /*gameMode*/ r.readU8();
    totalTicks = r.readU64();
    spawnX     = r.readF32();
    spawnY     = r.readF32();
    spawnZ     = r.readF32();

    return r.isValid();
}

// ========== Region file cache ==========

RegionFile& SaveManager::getRegion(int regionX, int regionZ) {
    RegionKey key{regionX, regionZ};
    auto it = regionCache_.find(key);
    if (it != regionCache_.end()) return *it->second;

    auto rf = std::make_unique<RegionFile>();
    std::string path = regionDir_ + "/" + RegionFile::regionFilename(regionX, regionZ);
    rf->open(path);

    auto& ref = *rf;
    regionCache_[key] = std::move(rf);
    return ref;
}

void SaveManager::closeAllRegions() {
    regionCache_.clear();  // unique_ptr destructors close files
}

// ========== Chunk save/load ==========

bool SaveManager::saveChunk(const Chunk& chunk) {
    int rx = RegionFile::chunkToRegion(chunk.chunkX());
    int rz = RegionFile::chunkToRegion(chunk.chunkZ());
    int lx = RegionFile::chunkToLocal(chunk.chunkX());
    int lz = RegionFile::chunkToLocal(chunk.chunkZ());

    auto data = ChunkSerializer::serialize(chunk);
    auto& region = getRegion(rx, rz);
    return region.writeChunk(lx, lz, data);
}

bool SaveManager::loadChunk(int chunkX, int chunkZ, Chunk& chunk) {
    int rx = RegionFile::chunkToRegion(chunkX);
    int rz = RegionFile::chunkToRegion(chunkZ);
    int lx = RegionFile::chunkToLocal(chunkX);
    int lz = RegionFile::chunkToLocal(chunkZ);

    // Check if region file exists before opening (avoid creating empty files)
    std::string path = regionDir_ + "/" + RegionFile::regionFilename(rx, rz);
    if (!fileExists(path)) return false;

    auto& region = getRegion(rx, rz);
    if (!region.hasChunk(lx, lz)) return false;

    auto data = region.readChunk(lx, lz);
    if (data.empty()) return false;

    return ChunkSerializer::deserialize(data.data(), data.size(), chunk);
}

bool SaveManager::hasChunk(int chunkX, int chunkZ) {
    int rx = RegionFile::chunkToRegion(chunkX);
    int rz = RegionFile::chunkToRegion(chunkZ);
    int lx = RegionFile::chunkToLocal(chunkX);
    int lz = RegionFile::chunkToLocal(chunkZ);

    std::string path = regionDir_ + "/" + RegionFile::regionFilename(rx, rz);
    if (!fileExists(path)) return false;

    auto& region = getRegion(rx, rz);
    return region.hasChunk(lx, lz);
}

int SaveManager::saveAllDirtyChunks(World& world) {
    int count = 0;
    for (auto& [key, chunk] : world.chunks()) {
        if (!chunk.isModified()) continue;
        if (saveChunk(chunk)) {
            chunk.clearModified();
            ++count;
        }
    }
    return count;
}

// ========== Entity save/load ==========

bool SaveManager::saveEntities(const EntityManager& mgr) {
    std::string tmpPath = entitiesPath() + ".tmp";
    {
        BinaryWriter w(tmpPath);
        if (!w.isValid()) {
            std::cerr << "[Save] Failed to open " << tmpPath << " for writing\n";
            return false;
        }

        w.writeHeader(VCFile::Type::Entities, VCFile::VERSION_ENTITIES);

        // Count living item entities
        uint32_t itemCount = 0;
        for (const auto& e : mgr.entities()) {
            if (e && e->alive && e->kind() == EntityKind::Item) ++itemCount;
        }
        w.writeU32(itemCount);

        for (const auto& e : mgr.entities()) {
            if (!e || !e->alive || e->kind() != EntityKind::Item) continue;
            const auto& item = static_cast<const ItemEntity&>(*e);

            // Position (3 floats)
            w.writeF32(item.position.x);
            w.writeF32(item.position.y);
            w.writeF32(item.position.z);
            // Velocity (3 floats)
            w.writeF32(item.velocity.x);
            w.writeF32(item.velocity.y);
            w.writeF32(item.velocity.z);
            // ItemStack
            w.writeU16(item.stack.id);
            w.writeU16(item.stack.count);
            w.writeU16(item.stack.durability);
            // Timers
            w.writeI32(item.pickupDelayTicks);
            w.writeI32(item.lifetimeTicks);
        }

        // Count living mob entities
        uint32_t mobCount = 0;
        for (const auto& e : mgr.entities()) {
            if (e && e->alive && e->kind() == EntityKind::Mob && !static_cast<const MobEntity&>(*e).isDying)
                ++mobCount;
        }
        w.writeU32(mobCount);

        for (const auto& e : mgr.entities()) {
            if (!e || !e->alive || e->kind() != EntityKind::Mob) continue;
            const auto& mob = static_cast<const MobEntity&>(*e);
            if (mob.isDying) continue;

            w.writeU8(static_cast<uint8_t>(mob.mobType));
            w.writeF32(mob.position.x);
            w.writeF32(mob.position.y);
            w.writeF32(mob.position.z);
            w.writeF32(mob.velocity.x);
            w.writeF32(mob.velocity.y);
            w.writeF32(mob.velocity.z);
            w.writeI32(mob.hp);
            w.writeF32(mob.bodyYaw);
        }

        w.close();
    }

    std::error_code ec;
    fs::rename(tmpPath, entitiesPath(), ec);
    if (ec) {
        std::cerr << "[Save] Failed to rename " << tmpPath << " → " << entitiesPath()
                  << ": " << ec.message() << "\n";
        fs::remove(tmpPath, ec);
        return false;
    }

    VLOG(DebugCat::Save, "Entities saved to %s", entitiesPath().c_str());
    return true;
}

bool SaveManager::loadEntities(EntityManager& mgr) {
    if (!fileExists(entitiesPath())) return false;

    BinaryReader r(entitiesPath());
    if (!r.isValid()) return false;

    uint16_t version;
    if (!r.readHeader(VCFile::Type::Entities, VCFile::VERSION_ENTITIES, version)) {
        std::cerr << "[Save] Invalid entities.dat header\n";
        return false;
    }

    uint32_t count = r.readU32();
    for (uint32_t i = 0; i < count && r.isValid(); ++i) {
        glm::vec3 pos;
        pos.x = r.readF32();
        pos.y = r.readF32();
        pos.z = r.readF32();

        glm::vec3 vel;
        vel.x = r.readF32();
        vel.y = r.readF32();
        vel.z = r.readF32();

        uint16_t itemId    = r.readU16();
        uint16_t itemCount = r.readU16();
        uint16_t itemDur   = r.readU16();

        int32_t pickupDelay = r.readI32();
        int32_t lifetime    = r.readI32();

        if (!r.isValid()) break;

        ItemStack stack{itemId, itemCount, itemDur};
        if (stack.isEmpty()) continue;

        auto item = std::make_unique<ItemEntity>();
        item->position     = pos;
        item->prevPosition = pos;
        item->velocity     = vel;
        item->stack        = stack;
        item->pickupDelayTicks = pickupDelay;
        item->lifetimeTicks    = lifetime;
        item->visualPhase  = std::fmod(pos.x + pos.z * 1.37f, 6.2831853f);
        item->prevVisualYaw = item->visualYaw;
        mgr.addEntity(std::move(item));
    }

    // Load mob entities (if data remains)
    if (r.isValid()) {
        uint32_t mobCount = r.readU32();
        for (uint32_t i = 0; i < mobCount && r.isValid(); ++i) {
            uint8_t typeU8 = r.readU8();
            if (typeU8 >= static_cast<uint8_t>(MobType::COUNT)) { r.skip(28); continue; }

            MobType type = static_cast<MobType>(typeU8);
            glm::vec3 pos;
            pos.x = r.readF32(); pos.y = r.readF32(); pos.z = r.readF32();
            glm::vec3 vel;
            vel.x = r.readF32(); vel.y = r.readF32(); vel.z = r.readF32();
            int32_t hp = r.readI32();
            float yaw = r.readF32();

            if (!r.isValid()) break;

            auto mob = std::make_unique<MobEntity>(type);
            mob->position = pos;
            mob->prevPosition = pos;
            mob->velocity = vel;
            mob->hp = hp;
            mob->bodyYaw = yaw;
            mob->prevBodyYaw = yaw;
            mgr.addEntity(std::move(mob));
        }
    }

    if (!r.isValid()) {
        std::cerr << "[Save] entities.dat appears truncated\n";
        return false;
    }

    VLOG(DebugCat::Save, "Loaded %u entities from %s", count, entitiesPath().c_str());
    return true;
}
