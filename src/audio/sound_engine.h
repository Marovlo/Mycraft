#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <random>
#include <mutex>
#include <glm/glm.hpp>

// 前向声明 miniaudio 类型，避免在头文件中包含巨大的 miniaudio.h
struct ma_engine;
struct ma_sound;

// ========== 音效材质类型（MC 原版方块音效分组） ==========
// 每种材质对应一组 dig/step/place 音效
enum class SoundMaterial : uint8_t {
    Stone,      // 石头、矿石、砖块等
    Wood,       // 木头、木板
    Gravel,     // 沙砾
    Grass,      // 草方块、泥土
    Sand,       // 沙子
    Snow,       // 雪
    Cloth,      // 羊毛
    Glass,      // 玻璃（只有破坏音效）
    Coral,      // 珊瑚
    WetGrass,   // 湿草
    Metal,      // 金属方块
    Count
};

// ========== 音效事件 ID ==========
// MC 原版中每个音效事件对应一组可选的 .ogg 文件，随机选一个播放
enum class SoundEventId : uint16_t {
    // --- 方块 dig（破坏）音效 ---
    DigStone = 0,
    DigWood,
    DigGravel,
    DigGrass,
    DigSand,
    DigSnow,
    DigCloth,
    DigCoral,
    DigWetGrass,

    // --- 方块 step（行走）音效 ---
    StepStone,
    StepWood,
    StepGravel,
    StepGrass,
    StepSand,
    StepSnow,
    StepCloth,
    StepCoral,
    StepWetGrass,

    // --- 方块 place（放置）音效（MC 中 place = dig 音效 + 不同音调） ---
    // 复用 dig 音效文件，播放时调整 pitch

    // --- 玩家 ---
    DamageFallBig,
    DamageFallSmall,
    DamageHit,          // hit1/hit2/hit3
    PlayerEat,          // random/eat1~3
    PlayerBurp,         // random/burp
    PlayerDrink,        // random/drink
    PlayerBreath,       // random/breath
    PlayerLevelUp,      // random/levelup
    PlayerXPOrb,        // random/orb

    // --- 交互 ---
    DoorOpen,
    DoorClose,
    ChestOpen,
    ChestClose,
    Click,              // random/click
    WoodClick,          // random/wood_click
    BowShoot,           // random/bow
    BowHit,             // random/bowhit1~4
    Explode,            // random/explode1~4
    Fizz,               // random/fizz
    Pop,                // random/pop
    Splash,             // random/splash
    GlassBreak,         // random/glass1~3
    AnvilUse,           // random/anvil_use
    AnvilLand,          // random/anvil_land
    AnvilBreak,         // random/anvil_break
    Fuse,               // random/fuse
    SuccessfulHit,      // random/successful_hit

    // --- UI ---
    UIButtonClick,      // ui/button/click1~4

    // --- 生物 ---
    MobCowSay,          // mob/cow/say1~4
    MobCowHurt,         // mob/cow/hurt1~3
    MobPigSay,          // mob/pig/say1~3
    MobPigDeath,        // mob/pig/death
    MobSheepSay,        // mob/sheep/say1~3
    MobChickenSay,      // mob/chicken/say1~3
    MobZombieSay,       // mob/zombie/say1~3
    MobZombieHurt,      // mob/zombie/hurt1~2
    MobZombieDeath,     // mob/zombie/death
    MobSkeletonSay,     // mob/skeleton/say1~3
    MobSkeletonHurt,    // mob/skeleton/hurt1~4
    MobSkeletonDeath,   // mob/skeleton/death
    MobSpiderSay,       // mob/spider/say1~4
    MobSpiderDeath,     // mob/spider/death
    MobCreeperDeath,    // mob/creeper/death

    // --- 物品/经验 ---
    ItemPickup,         // random/pop
    XPOrbPickup,        // random/orb

    Count
};

// ========== 音效事件定义 ==========
// 一个音效事件包含多个候选 .ogg 文件路径，播放时随机选一个
struct SoundEventDef {
    std::vector<std::string> files;  // 相对于 sounds/ 目录的路径
    float volume   = 1.0f;          // 默认音量
    float pitch    = 1.0f;          // 默认音调
    float pitchVariance = 0.0f;     // 音调随机偏移范围 (±)
    bool  positional = true;        // 是否为 3D 空间音效
};

// ========== 音效实例（正在播放的音效） ==========
struct SoundInstance {
    ma_sound* sound = nullptr;
    bool      inUse = false;
};

// ========== 音效引擎 ==========
// 基于 miniaudio 的 3D 空间音效引擎
// 支持：音效池、3D 定位、音量分类控制、随机变体选择
class SoundEngine {
public:
    SoundEngine();
    ~SoundEngine();

    // 禁止拷贝
    SoundEngine(const SoundEngine&) = delete;
    SoundEngine& operator=(const SoundEngine&) = delete;

    // 初始化音频引擎
    // @param soundsBasePath: assets/sounds/ 目录的绝对路径
    bool init(const std::string& soundsBasePath);
    void shutdown();

    // 每帧更新：清理已播放完毕的音效实例
    void update();

    // --- 播放音效 ---

    // 在世界坐标播放 3D 音效
    void play(SoundEventId event, const glm::vec3& position,
              float volumeScale = 1.0f, float pitchScale = 1.0f);

    // 播放非定位音效（UI 音效等）
    void play2D(SoundEventId event, float volumeScale = 1.0f, float pitchScale = 1.0f);

    // --- 方块音效便捷接口 ---

    // 播放方块破坏音效
    void playBlockBreak(SoundMaterial mat, const glm::vec3& pos);
    // 播放方块放置音效
    void playBlockPlace(SoundMaterial mat, const glm::vec3& pos);
    // 播放行走音效
    void playStep(SoundMaterial mat, const glm::vec3& pos);

    // --- 听者（摄像机）位置更新 ---
    void setListenerPosition(const glm::vec3& pos, const glm::vec3& forward, const glm::vec3& up);

    // --- 音量控制 ---
    void setMasterVolume(float vol);   // 0.0 ~ 1.0
    void setSFXVolume(float vol);
    void setMusicVolume(float vol);
    float getMasterVolume() const { return masterVolume_; }
    float getSFXVolume() const { return sfxVolume_; }
    float getMusicVolume() const { return musicVolume_; }

    bool isInitialized() const { return initialized_; }

    // 获取底层 miniaudio 引擎指针（供 MusicManager 使用）
    ma_engine* getEngine() const { return engine_; }

    // 获取材质对应的 dig 音效事件
    static SoundEventId getDigEvent(SoundMaterial mat);
    // 获取材质对应的 step 音效事件
    static SoundEventId getStepEvent(SoundMaterial mat);

private:
    // 注册所有音效事件定义
    void registerSoundEvents();

    // 从音效池获取一个空闲的 ma_sound 实例
    SoundInstance* acquireInstance();

    // 获取音效事件的随机文件路径
    std::string pickRandomFile(SoundEventId event);

    bool initialized_ = false;
    std::string basePath_;

    // miniaudio 引擎（pImpl 避免头文件暴露 miniaudio）
    ma_engine* engine_ = nullptr;

    // 音效事件注册表
    std::unordered_map<uint16_t, SoundEventDef> eventDefs_;

    // 音效实例池（限制同时播放数量，避免音频过载）
    static constexpr int MAX_CONCURRENT_SOUNDS = 64;
    std::vector<SoundInstance> soundPool_;

    // 音量
    float masterVolume_ = 1.0f;
    float sfxVolume_    = 1.0f;
    float musicVolume_  = 0.5f;

    // 随机数生成器
    std::mt19937 rng_;
    std::mutex   mutex_;
};

// ========== 全局音效引擎访问 ==========
// 方便在各处调用，避免到处传递指针
SoundEngine& getSoundEngine();
