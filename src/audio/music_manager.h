#pragma once

#include "sound_engine.h"
#include <string>
#include <vector>
#include <random>
#include <mutex>

// 前向声明
struct ma_sound;
struct ma_engine;

// ========== 背景音乐管理器 ==========
// MC 原版背景音乐系统：
// - 在游戏中随机间隔播放背景音乐
// - 支持不同场景的音乐列表（主菜单、游戏内、下界、末地等）
// - 淡入淡出切换
// - 当前版本：保留完整接口，但不包含版权音乐文件
//   用户可以将自己的 .ogg 音乐文件放入 assets/sounds/music/ 目录

enum class MusicContext : uint8_t {
    Menu,       // 主菜单
    Game,       // 游戏内（主世界）
    Creative,   // 创造模式
    Nether,     // 下界
    End,        // 末地
    Underwater, // 水下
    Count
};

class MusicManager {
public:
    MusicManager();
    ~MusicManager();

    // 禁止拷贝
    MusicManager(const MusicManager&) = delete;
    MusicManager& operator=(const MusicManager&) = delete;

    // 初始化，扫描 music/ 目录下的可用音乐文件
    // @param engine: miniaudio 引擎指针（从 SoundEngine 获取）
    // @param musicBasePath: assets/sounds/music/ 目录路径
    bool init(ma_engine* engine, const std::string& musicBasePath);
    void shutdown();

    // 每帧更新：处理淡入淡出、计时、自动播放下一首
    void update(float dt);

    // 设置当前音乐场景（切换时会淡出当前曲目，淡入新场景曲目）
    void setContext(MusicContext ctx);

    // 手动控制
    void play();       // 立即播放当前场景的随机曲目
    void stop();       // 淡出停止
    void pause();
    void resume();

    // 音量（受 SoundEngine 的 musicVolume 控制）
    void setVolume(float vol);
    float getVolume() const { return volume_; }

    // 是否正在播放
    bool isPlaying() const { return playing_; }

    // 是否有可用的音乐文件
    bool hasMusic() const;

private:
    // 选择当前场景的随机曲目并开始播放
    void playNext();

    // 淡入淡出
    void updateFade(float dt);

    ma_engine* engine_ = nullptr;
    ma_sound*  currentSound_ = nullptr;
    std::string basePath_;

    // 每个场景的音乐文件列表
    std::vector<std::string> musicFiles_[static_cast<int>(MusicContext::Count)];

    MusicContext currentContext_ = MusicContext::Game;
    bool   initialized_ = false;
    bool   playing_     = false;
    bool   paused_      = false;
    bool   enabled_     = true;   // 总开关

    float  volume_      = 0.5f;

    // 淡入淡出
    enum class FadeState { None, FadingIn, FadingOut, FadeOutThenSwitch };
    FadeState fadeState_ = FadeState::None;
    float fadeTimer_     = 0.0f;
    float fadeDuration_  = 3.0f;  // 淡入淡出时长（秒）

    // MC 原版：音乐之间有随机间隔（10~20 分钟）
    float silenceTimer_    = 0.0f;
    float silenceDuration_ = 0.0f;
    bool  waitingForNext_  = false;

    // 随机数
    std::mt19937 rng_;
    std::mutex   mutex_;

    // 生成下一首的等待时间（MC 原版：600~1200 秒 = 10~20 分钟）
    float randomSilenceDuration();
};
