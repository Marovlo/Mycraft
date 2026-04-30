#include "sound_engine.h"
#include "miniaudio.h"
#include "core/debug.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>

// ========== 全局单例 ==========
static SoundEngine g_soundEngine;

SoundEngine& getSoundEngine() {
    return g_soundEngine;
}

// ========== 构造 / 析构 ==========

SoundEngine::SoundEngine()
    : rng_(std::random_device{}())
{
}

SoundEngine::~SoundEngine() {
    shutdown();
}

// ========== 初始化 ==========

bool SoundEngine::init(const std::string& soundsBasePath) {
    if (initialized_) return true;

    basePath_ = soundsBasePath;
    // 确保路径以 / 结尾
    if (!basePath_.empty() && basePath_.back() != '/') {
        basePath_ += '/';
    }

    // 创建 miniaudio 引擎
    engine_ = new ma_engine();

    ma_engine_config config = ma_engine_config_init();
    config.channels = 2;           // 立体声输出
    config.sampleRate = 44100;
    config.listenerCount = 1;      // 单听者（玩家）

    ma_result result = ma_engine_init(&config, engine_);
    if (result != MA_SUCCESS) {
        std::cerr << "[SoundEngine] 初始化 miniaudio 引擎失败: " << result << std::endl;
        delete engine_;
        engine_ = nullptr;
        return false;
    }

    // 初始化音效实例池
    soundPool_.resize(MAX_CONCURRENT_SOUNDS);
    for (auto& inst : soundPool_) {
        inst.sound = new ma_sound();
        inst.inUse = false;
    }

    // 注册所有音效事件
    registerSoundEvents();

    initialized_ = true;
    std::cout << "[SoundEngine] 初始化成功，音效基础路径: " << basePath_ << std::endl;
    return true;
}

void SoundEngine::shutdown() {
    if (!initialized_) return;

    // 停止并释放所有音效实例
    for (auto& inst : soundPool_) {
        if (inst.inUse && inst.sound) {
            ma_sound_stop(inst.sound);
            ma_sound_uninit(inst.sound);
        }
        delete inst.sound;
        inst.sound = nullptr;
        inst.inUse = false;
    }
    soundPool_.clear();

    // 关闭引擎
    if (engine_) {
        ma_engine_uninit(engine_);
        delete engine_;
        engine_ = nullptr;
    }

    initialized_ = false;
}

// ========== 每帧更新 ==========

void SoundEngine::update() {
    if (!initialized_) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // 回收已播放完毕的音效实例
    for (auto& inst : soundPool_) {
        if (inst.inUse && inst.sound) {
            if (!ma_sound_is_playing(inst.sound)) {
                ma_sound_uninit(inst.sound);
                inst.inUse = false;
            }
        }
    }
}

// ========== 听者位置 ==========

void SoundEngine::setListenerPosition(const glm::vec3& pos, const glm::vec3& forward, const glm::vec3& up) {
    if (!initialized_) return;

    ma_engine_listener_set_position(engine_, 0, pos.x, pos.y, pos.z);
    ma_engine_listener_set_direction(engine_, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(engine_, 0, up.x, up.y, up.z);
}

// ========== 音量控制 ==========

void SoundEngine::setMasterVolume(float vol) {
    masterVolume_ = std::clamp(vol, 0.0f, 1.0f);
    if (engine_) {
        ma_engine_set_volume(engine_, masterVolume_);
    }
}

void SoundEngine::setSFXVolume(float vol) {
    sfxVolume_ = std::clamp(vol, 0.0f, 1.0f);
}

void SoundEngine::setMusicVolume(float vol) {
    musicVolume_ = std::clamp(vol, 0.0f, 1.0f);
}

// ========== 播放音效 ==========

void SoundEngine::play(SoundEventId event, const glm::vec3& position,
                       float volumeScale, float pitchScale) {
    if (!initialized_) return;

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = eventDefs_.find(static_cast<uint16_t>(event));
    if (it == eventDefs_.end() || it->second.files.empty()) return;

    const auto& def = it->second;
    std::string filePath = basePath_ + pickRandomFile(event);

    // 检查文件是否存在
    if (!std::filesystem::exists(filePath)) {
        return; // 静默跳过缺失的音效文件
    }

    SoundInstance* inst = acquireInstance();
    if (!inst) return; // 音效池已满

    // 初始化音效
    ma_uint32 flags = MA_SOUND_FLAG_DECODE; // 解码到内存，减少延迟
    // 非定位音效关闭空间化
    if (!def.positional) {
        flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
    }

    ma_result result = ma_sound_init_from_file(engine_, filePath.c_str(),
                                                flags, nullptr, nullptr, inst->sound);
    if (result != MA_SUCCESS) {
        inst->inUse = false;
        return;
    }

    // 设置 3D 位置
    if (def.positional) {
        ma_sound_set_spatialization_enabled(inst->sound, MA_TRUE);
        ma_sound_set_position(inst->sound, position.x, position.y, position.z);
        ma_sound_set_min_distance(inst->sound, 1.0f);
        ma_sound_set_max_distance(inst->sound, 16.0f);  // MC 原版音效衰减距离
        ma_sound_set_attenuation_model(inst->sound, ma_attenuation_model_linear);
        ma_sound_set_rolloff(inst->sound, 1.0f);
    }

    // 音量 = 事件默认音量 × SFX音量 × 调用者缩放
    float finalVolume = def.volume * sfxVolume_ * volumeScale;
    ma_sound_set_volume(inst->sound, finalVolume);

    // 音调 = 事件默认音调 × 调用者缩放 ± 随机偏移
    float pitchVariance = 0.0f;
    if (def.pitchVariance > 0.0f) {
        std::uniform_real_distribution<float> dist(-def.pitchVariance, def.pitchVariance);
        pitchVariance = dist(rng_);
    }
    float finalPitch = def.pitch * pitchScale + pitchVariance;
    // 确保 pitch 不为负或零
    if (finalPitch < 0.1f) finalPitch = 0.1f;
    ma_sound_set_pitch(inst->sound, finalPitch);

    // 播放
    ma_sound_start(inst->sound);
    inst->inUse = true;
}

void SoundEngine::play2D(SoundEventId event, float volumeScale, float pitchScale) {
    if (!initialized_) return;

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = eventDefs_.find(static_cast<uint16_t>(event));
    if (it == eventDefs_.end() || it->second.files.empty()) return;

    const auto& def = it->second;
    std::string filePath = basePath_ + pickRandomFile(event);

    if (!std::filesystem::exists(filePath)) {
        return;
    }

    SoundInstance* inst = acquireInstance();
    if (!inst) return;

    ma_result result = ma_sound_init_from_file(engine_, filePath.c_str(),
                                                MA_SOUND_FLAG_DECODE, nullptr, nullptr, inst->sound);
    if (result != MA_SUCCESS) {
        inst->inUse = false;
        return;
    }

    // 2D 音效：关闭空间化
    ma_sound_set_spatialization_enabled(inst->sound, MA_FALSE);

    float finalVolume = def.volume * sfxVolume_ * volumeScale;
    ma_sound_set_volume(inst->sound, finalVolume);

    float pitchVariance = 0.0f;
    if (def.pitchVariance > 0.0f) {
        std::uniform_real_distribution<float> dist(-def.pitchVariance, def.pitchVariance);
        pitchVariance = dist(rng_);
    }
    float finalPitch = def.pitch * pitchScale + pitchVariance;
    ma_sound_set_pitch(inst->sound, finalPitch);

    ma_sound_start(inst->sound);
    inst->inUse = true;
}

// ========== 方块音效便捷接口 ==========

void SoundEngine::playBlockBreak(SoundMaterial mat, const glm::vec3& pos) {
    // MC 原版：破坏音效 pitch 在 0.8 附近随机
    play(getDigEvent(mat), pos, 1.0f, 0.8f);
}

void SoundEngine::playBlockPlace(SoundMaterial mat, const glm::vec3& pos) {
    // MC 原版：放置音效 = dig 音效 + pitch 略高于破坏
    play(getDigEvent(mat), pos, 0.8f, 0.9f);
}

void SoundEngine::playStep(SoundMaterial mat, const glm::vec3& pos) {
    play(getStepEvent(mat), pos, 0.15f, 1.0f); // MC 原版脚步声音量较低
}

// ========== 材质 → 音效事件映射 ==========

SoundEventId SoundEngine::getDigEvent(SoundMaterial mat) {
    switch (mat) {
        case SoundMaterial::Stone:    return SoundEventId::DigStone;
        case SoundMaterial::Wood:     return SoundEventId::DigWood;
        case SoundMaterial::Gravel:   return SoundEventId::DigGravel;
        case SoundMaterial::Grass:    return SoundEventId::DigGrass;
        case SoundMaterial::Sand:     return SoundEventId::DigSand;
        case SoundMaterial::Snow:     return SoundEventId::DigSnow;
        case SoundMaterial::Cloth:    return SoundEventId::DigCloth;
        case SoundMaterial::Glass:    return SoundEventId::DigStone; // 玻璃破坏用石头音效
        case SoundMaterial::Coral:    return SoundEventId::DigCoral;
        case SoundMaterial::WetGrass: return SoundEventId::DigWetGrass;
        case SoundMaterial::Metal:    return SoundEventId::DigStone; // 金属暂用石头
        default:                      return SoundEventId::DigStone;
    }
}

SoundEventId SoundEngine::getStepEvent(SoundMaterial mat) {
    switch (mat) {
        case SoundMaterial::Stone:    return SoundEventId::StepStone;
        case SoundMaterial::Wood:     return SoundEventId::StepWood;
        case SoundMaterial::Gravel:   return SoundEventId::StepGravel;
        case SoundMaterial::Grass:    return SoundEventId::StepGrass;
        case SoundMaterial::Sand:     return SoundEventId::StepSand;
        case SoundMaterial::Snow:     return SoundEventId::StepSnow;
        case SoundMaterial::Cloth:    return SoundEventId::StepCloth;
        case SoundMaterial::Glass:    return SoundEventId::StepStone;
        case SoundMaterial::Coral:    return SoundEventId::StepCoral;
        case SoundMaterial::WetGrass: return SoundEventId::StepWetGrass;
        case SoundMaterial::Metal:    return SoundEventId::StepStone;
        default:                      return SoundEventId::StepStone;
    }
}

// ========== 内部辅助 ==========

SoundInstance* SoundEngine::acquireInstance() {
    // 查找空闲实例
    for (auto& inst : soundPool_) {
        if (!inst.inUse) {
            return &inst;
        }
    }

    // 池满：找到最早开始播放的实例并强制回收
    // 简单策略：回收第一个找到的实例
    auto& oldest = soundPool_[0];
    if (oldest.sound) {
        ma_sound_stop(oldest.sound);
        ma_sound_uninit(oldest.sound);
    }
    oldest.inUse = false;
    return &oldest;
}

std::string SoundEngine::pickRandomFile(SoundEventId event) {
    auto it = eventDefs_.find(static_cast<uint16_t>(event));
    if (it == eventDefs_.end() || it->second.files.empty()) return "";

    const auto& files = it->second.files;
    if (files.size() == 1) return files[0];

    std::uniform_int_distribution<size_t> dist(0, files.size() - 1);
    return files[dist(rng_)];
}

// ========== 音效事件注册 ==========

// 辅助宏：生成编号文件列表
static std::vector<std::string> makeNumberedFiles(const std::string& prefix, int count,
                                                   const std::string& ext = ".ogg") {
    std::vector<std::string> files;
    files.reserve(count);
    for (int i = 1; i <= count; ++i) {
        files.push_back(prefix + std::to_string(i) + ext);
    }
    return files;
}

void SoundEngine::registerSoundEvents() {
    auto reg = [this](SoundEventId id, SoundEventDef def) {
        eventDefs_[static_cast<uint16_t>(id)] = std::move(def);
    };

    // ===== 方块 dig（破坏）音效 =====
    reg(SoundEventId::DigStone,    {makeNumberedFiles("dig/stone", 4),    1.0f, 1.0f, 0.1f, true});
    reg(SoundEventId::DigWood,     {makeNumberedFiles("dig/wood", 4),     1.0f, 1.0f, 0.1f, true});
    reg(SoundEventId::DigGravel,   {makeNumberedFiles("dig/gravel", 4),   1.0f, 1.0f, 0.1f, true});
    reg(SoundEventId::DigGrass,    {makeNumberedFiles("dig/grass", 4),    1.0f, 1.0f, 0.1f, true});
    reg(SoundEventId::DigSand,     {makeNumberedFiles("dig/sand", 4),     1.0f, 1.0f, 0.1f, true});
    reg(SoundEventId::DigSnow,     {makeNumberedFiles("dig/snow", 4),     1.0f, 1.0f, 0.1f, true});
    reg(SoundEventId::DigCloth,    {makeNumberedFiles("dig/cloth", 4),    1.0f, 1.0f, 0.1f, true});
    reg(SoundEventId::DigCoral,    {makeNumberedFiles("dig/coral", 4),    1.0f, 1.0f, 0.1f, true});
    reg(SoundEventId::DigWetGrass, {makeNumberedFiles("dig/wet_grass", 4),1.0f, 1.0f, 0.1f, true});

    // ===== 方块 step（行走）音效 =====
    reg(SoundEventId::StepStone,    {makeNumberedFiles("step/stone", 6),    0.15f, 1.0f, 0.1f, true});
    reg(SoundEventId::StepWood,     {makeNumberedFiles("step/wood", 6),     0.15f, 1.0f, 0.1f, true});
    reg(SoundEventId::StepGravel,   {makeNumberedFiles("step/gravel", 4),   0.15f, 1.0f, 0.1f, true});
    reg(SoundEventId::StepGrass,    {makeNumberedFiles("step/grass", 6),    0.15f, 1.0f, 0.1f, true});
    reg(SoundEventId::StepSand,     {makeNumberedFiles("step/sand", 5),     0.15f, 1.0f, 0.1f, true});
    reg(SoundEventId::StepSnow,     {makeNumberedFiles("step/snow", 4),     0.15f, 1.0f, 0.1f, true});
    reg(SoundEventId::StepCloth,    {makeNumberedFiles("step/cloth", 4),    0.15f, 1.0f, 0.1f, true});
    reg(SoundEventId::StepCoral,    {makeNumberedFiles("step/coral", 6),    0.15f, 1.0f, 0.1f, true});
    reg(SoundEventId::StepWetGrass, {makeNumberedFiles("step/wet_grass", 6),0.15f, 1.0f, 0.1f, true});

    // ===== 玩家音效 =====
    reg(SoundEventId::DamageFallBig,   {{"damage/fallbig.ogg"},   1.0f, 1.0f, 0.0f, true});
    reg(SoundEventId::DamageFallSmall, {{"damage/fallsmall.ogg"}, 1.0f, 1.0f, 0.0f, true});
    reg(SoundEventId::DamageHit,       {makeNumberedFiles("damage/hit", 3), 1.0f, 1.0f, 0.1f, true});

    reg(SoundEventId::PlayerEat,     {makeNumberedFiles("random/eat", 3),  0.5f, 1.0f, 0.1f, false});
    reg(SoundEventId::PlayerBurp,    {{"random/burp.ogg"},                 0.5f, 1.0f, 0.0f, false});
    reg(SoundEventId::PlayerDrink,   {{"random/drink.ogg"},                0.5f, 1.0f, 0.0f, false});
    reg(SoundEventId::PlayerBreath,  {{"random/breath.ogg"},               1.0f, 1.0f, 0.0f, false});
    reg(SoundEventId::PlayerLevelUp, {{"random/levelup.ogg"},              1.0f, 1.0f, 0.0f, false});
    reg(SoundEventId::PlayerXPOrb,   {{"random/orb.ogg"},                  0.3f, 1.0f, 0.2f, false});

    // ===== 交互音效 =====
    reg(SoundEventId::DoorOpen,    {{"random/door_open.ogg"},    1.0f, 1.0f, 0.1f, true});
    reg(SoundEventId::DoorClose,   {{"random/door_close.ogg"},   1.0f, 1.0f, 0.1f, true});
    reg(SoundEventId::ChestOpen,   {{"random/chestopen.ogg"},    0.5f, 1.0f, 0.0f, true});
    reg(SoundEventId::ChestClose,  {{"random/chestclosed.ogg"},  0.5f, 1.0f, 0.0f, true});
    reg(SoundEventId::Click,       {{"random/click.ogg"},        0.3f, 1.0f, 0.0f, false});
    reg(SoundEventId::WoodClick,   {{"random/wood_click.ogg"},   0.3f, 1.0f, 0.0f, false});
    reg(SoundEventId::BowShoot,    {{"random/bow.ogg"},          1.0f, 1.0f, 0.1f, true});
    reg(SoundEventId::BowHit,      {makeNumberedFiles("random/bowhit", 4), 1.0f, 1.0f, 0.1f, true});
    reg(SoundEventId::Explode,     {makeNumberedFiles("random/explode", 4), 1.0f, 1.0f, 0.1f, true});
    reg(SoundEventId::Fizz,        {{"random/fizz.ogg"},         0.5f, 1.0f, 0.0f, true});
    reg(SoundEventId::Pop,         {{"random/pop.ogg"},          0.3f, 1.0f, 0.2f, true});
    reg(SoundEventId::Splash,      {{"random/splash.ogg"},       1.0f, 1.0f, 0.0f, true});
    reg(SoundEventId::GlassBreak,  {makeNumberedFiles("random/glass", 3), 1.0f, 1.0f, 0.1f, true});
    reg(SoundEventId::AnvilUse,    {{"random/anvil_use.ogg"},    1.0f, 1.0f, 0.0f, true});
    reg(SoundEventId::AnvilLand,   {{"random/anvil_land.ogg"},   1.0f, 1.0f, 0.0f, true});
    reg(SoundEventId::AnvilBreak,  {{"random/anvil_break.ogg"},  1.0f, 1.0f, 0.0f, true});
    reg(SoundEventId::Fuse,        {{"random/fuse.ogg"},         1.0f, 1.0f, 0.0f, true});
    reg(SoundEventId::SuccessfulHit, {{"random/successful_hit.ogg"}, 0.5f, 1.0f, 0.0f, false});

    // ===== UI 音效 =====
    // UI 按钮点击音效
    std::vector<std::string> uiClickFiles;
    std::string uiButtonDir = basePath_ + "ui/button/";
    if (std::filesystem::exists(uiButtonDir) && std::filesystem::is_directory(uiButtonDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(uiButtonDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".ogg") {
                uiClickFiles.push_back("ui/button/" + entry.path().filename().string());
            }
        }
    }
    if (uiClickFiles.empty()) {
        // 回退到 random/click
        uiClickFiles.push_back("random/click.ogg");
    }
    reg(SoundEventId::UIButtonClick, {uiClickFiles, 0.5f, 1.0f, 0.0f, false});

    // ===== 生物音效 =====
    reg(SoundEventId::MobCowSay,      {makeNumberedFiles("mob/cow/say", 4),      0.4f, 1.0f, 0.1f, true});
    reg(SoundEventId::MobCowHurt,     {makeNumberedFiles("mob/cow/hurt", 3),     0.4f, 1.0f, 0.1f, true});
    reg(SoundEventId::MobPigSay,      {makeNumberedFiles("mob/pig/say", 3),      0.4f, 1.0f, 0.1f, true});
    reg(SoundEventId::MobPigDeath,    {{"mob/pig/death.ogg"},                    0.4f, 1.0f, 0.0f, true});
    reg(SoundEventId::MobSheepSay,    {makeNumberedFiles("mob/sheep/say", 3),    0.4f, 1.0f, 0.1f, true});
    reg(SoundEventId::MobChickenSay,  {makeNumberedFiles("mob/chicken/say", 3),  0.3f, 1.0f, 0.1f, true});
    reg(SoundEventId::MobZombieSay,   {makeNumberedFiles("mob/zombie/say", 3),   0.4f, 1.0f, 0.1f, true});
    reg(SoundEventId::MobZombieHurt,  {makeNumberedFiles("mob/zombie/hurt", 2),  0.4f, 1.0f, 0.1f, true});
    reg(SoundEventId::MobZombieDeath, {{"mob/zombie/death.ogg"},                 0.4f, 1.0f, 0.0f, true});
    reg(SoundEventId::MobSkeletonSay, {makeNumberedFiles("mob/skeleton/say", 3), 0.4f, 1.0f, 0.1f, true});
    reg(SoundEventId::MobSkeletonHurt,{makeNumberedFiles("mob/skeleton/hurt", 4),0.4f, 1.0f, 0.1f, true});
    reg(SoundEventId::MobSkeletonDeath,{{"mob/skeleton/death.ogg"},              0.4f, 1.0f, 0.0f, true});
    reg(SoundEventId::MobSpiderSay,   {makeNumberedFiles("mob/spider/say", 4),   0.4f, 1.0f, 0.1f, true});
    reg(SoundEventId::MobSpiderDeath, {{"mob/spider/death.ogg"},                 0.4f, 1.0f, 0.0f, true});
    reg(SoundEventId::MobCreeperDeath,{{"mob/creeper/death.ogg"},                0.4f, 1.0f, 0.0f, true});

    // ===== 物品/经验音效 =====
    reg(SoundEventId::ItemPickup,     {{"random/pop.ogg"},                       0.3f, 1.0f, 0.2f, true});
    reg(SoundEventId::XPOrbPickup,    {{"random/orb.ogg"},                       0.3f, 1.0f, 0.2f, false});

    // ===== 环境音效 =====
    // 洞穴环境音效（cave1~23.ogg）
    {
        std::vector<std::string> caveFiles;
        for (int i = 1; i <= 23; ++i) {
            std::string path = "ambient/cave/cave" + std::to_string(i) + ".ogg";
            if (std::filesystem::exists(basePath_ + path)) {
                caveFiles.push_back(path);
            }
        }
        if (!caveFiles.empty()) {
            reg(SoundEventId::AmbientCave, {caveFiles, 0.7f, 1.0f, 0.0f, true});
        }
    }
    // 雨声（rain1~8.ogg）
    {
        std::vector<std::string> rainFiles;
        for (int i = 1; i <= 8; ++i) {
            std::string path = "ambient/weather/rain" + std::to_string(i) + ".ogg";
            if (std::filesystem::exists(basePath_ + path)) {
                rainFiles.push_back(path);
            }
        }
        if (!rainFiles.empty()) {
            reg(SoundEventId::AmbientRain, {rainFiles, 0.5f, 1.0f, 0.0f, false});
        }
    }
    // 雷声（thunder1~3.ogg）
    {
        std::vector<std::string> thunderFiles;
        for (int i = 1; i <= 3; ++i) {
            std::string path = "ambient/weather/thunder" + std::to_string(i) + ".ogg";
            if (std::filesystem::exists(basePath_ + path)) {
                thunderFiles.push_back(path);
            }
        }
        if (!thunderFiles.empty()) {
            reg(SoundEventId::AmbientThunder, {thunderFiles, 1.0f, 1.0f, 0.1f, true});
        }
    }

    std::cout << "[SoundEngine] 注册了 " << eventDefs_.size() << " 个音效事件" << std::endl;
}
