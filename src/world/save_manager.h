#pragma once

#include "region_file.h"

#include <string>
#include <cstdint>
#include <unordered_map>
#include <memory>

class Player;
class Inventory;
class World;
class Chunk;
class TerrainGenerator;

// ========== SaveManager ==========
// Central manager for all world save/load operations.
// Handles file paths, directory creation, and orchestrates serialization.
//
// Directory structure:
//   saves/<worldName>/
//     level.dat           — world metadata (seed, name, ticks, etc.)
//     players/<name>.dat  — per-player state + inventory (server-authoritative)
//     region/
//       r.X.Z.mca         — region files (32×32 chunks each)

class SaveManager {
public:
    SaveManager() = default;
    ~SaveManager();

    // Set the active world directory. Creates directories if needed.
    // worldName: e.g., "Default World"
    // basePath: e.g., "saves" (relative to executable or absolute)
    void setWorld(const std::string& worldName, const std::string& basePath = "saves");

    const std::string& getWorldDir() const { return worldDir_; }
    const std::string& getWorldName() const { return worldName_; }
    const std::string& getRegionDir() const { return regionDir_; }

    // --- Player data (server-authoritative, keyed by player name) ---
    // Save/load player data for a specific named player.
    // Path: <worldDir>/players/<playerName>.dat
    bool savePlayerByName(const std::string& playerName,
                          const Player& player, const Inventory& inventory);
    bool loadPlayerByName(const std::string& playerName,
                          Player& player, Inventory& inventory);

    // Legacy single-player shortcuts (uses "player" as the name)
    bool savePlayer(const Player& player, const Inventory& inventory);
    bool loadPlayer(Player& player, Inventory& inventory);

    // --- World metadata (level.dat) ---
    bool saveLevelData(int64_t seed, uint64_t totalTicks,
                       float spawnX, float spawnY, float spawnZ,
                       const std::string& worldName);
    bool loadLevelData(int64_t& seed, uint64_t& totalTicks,
                       float& spawnX, float& spawnY, float& spawnZ,
                       std::string& worldName);

    // --- Entity data (dropped items) ---
    bool saveEntities(const class EntityManager& mgr);
    bool loadEntities(class EntityManager& mgr);

    // --- Chunk data (region files) ---
    // Save a single modified chunk to its region file.
    bool saveChunk(const Chunk& chunk);

    // Try to load a chunk from its region file. Returns true if found and loaded.
    // On success, chunk is filled with block data (but NO lighting — caller must init).
    bool loadChunk(int chunkX, int chunkZ, Chunk& chunk);

    // Check if a chunk has saved data on disk (i.e., was ever modified by a player).
    // Does NOT load the chunk into memory.
    bool hasChunk(int chunkX, int chunkZ);

    // Save all modified chunks from the world.
    // Returns number of chunks saved.
    int saveAllDirtyChunks(World& world);

    // Flush and close all cached region files.
    void closeAllRegions();

    // --- Helpers ---
    static bool ensureDirectory(const std::string& path);
    static bool fileExists(const std::string& path);

private:
    std::string worldName_;
    std::string worldDir_;         // e.g., "saves/Default World"
    std::string regionDir_;        // e.g., "saves/Default World/region"

    // Region file cache (key = packed regionX<<16|regionZ)
    struct RegionKey {
        int x, z;
        bool operator==(const RegionKey& o) const { return x == o.x && z == o.z; }
    };
    struct RegionKeyHash {
        size_t operator()(const RegionKey& k) const {
            return std::hash<int>()(k.x) ^ (std::hash<int>()(k.z) * 2654435761u);
        }
    };
    std::unordered_map<RegionKey, std::unique_ptr<RegionFile>, RegionKeyHash> regionCache_;

    RegionFile& getRegion(int regionX, int regionZ);

    std::string playerPath() const;
    std::string playerPathByName(const std::string& playerName) const;
    std::string levelPath() const;
    std::string entitiesPath() const;
};
