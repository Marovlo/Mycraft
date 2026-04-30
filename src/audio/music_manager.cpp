#include "music_manager.h"
#include "miniaudio.h"

#include <algorithm>
#include <filesystem>
#include <iostream>

MusicManager::MusicManager()
    : rng_(std::random_device{}())
{
}

MusicManager::~MusicManager() {
    shutdown();
}

bool MusicManager::init(ma_engine* engine, const std::string& musicBasePath) {
    if (initialized_) return true;
    if (!engine) return false;

    engine_ = engine;
    basePath_ = musicBasePath;
    if (!basePath_.empty() && basePath_.back() != '/') {
        basePath_ += '/';
    }

    // 分配当前播放的 sound 对象
    currentSound_ = new ma_sound();

    // 扫描 music/ 目录下的子目录，按场景分类
    // 目录结构（MC 原版）：
    //   music/game/         → Game 场景
    //   music/game/creative/ → Creative 场景
    //   music/game/nether/  → Nether 场景
    //   music/game/end/     → End 场景
    //   music/game/water/   → Underwater 场景
    //   music/menu/         → Menu 场景

    auto scanDir = [this](const std::string& dir, MusicContext ctx) {
        std::string fullDir = basePath_ + dir;
        if (!std::filesystem::exists(fullDir)) return;

        for (const auto& entry : std::filesystem::directory_iterator(fullDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".ogg") {
                musicFiles_[static_cast<int>(ctx)].push_back(entry.path().string());
            }
        }
    };

    // 扫描各场景目录
    scanDir("game/", MusicContext::Game);
    scanDir("game/creative/", MusicContext::Creative);
    scanDir("game/nether/", MusicContext::Nether);
    scanDir("game/end/", MusicContext::End);
    scanDir("game/water/", MusicContext::Underwater);
    scanDir("menu/", MusicContext::Menu);

    // 统计
    int totalTracks = 0;
    for (int i = 0; i < static_cast<int>(MusicContext::Count); ++i) {
        totalTracks += static_cast<int>(musicFiles_[i].size());
    }

    std::cout << "[MusicManager] 初始化完成，共发现 " << totalTracks << " 首音乐" << std::endl;
    for (int i = 0; i < static_cast<int>(MusicContext::Count); ++i) {
        if (!musicFiles_[i].empty()) {
            const char* names[] = {"Menu", "Game", "Creative", "Nether", "End", "Underwater"};
            std::cout << "  " << names[i] << ": " << musicFiles_[i].size() << " 首" << std::endl;
        }
    }

    // 初始等待时间（首次播放前等待较短时间）
    silenceDuration_ = 30.0f; // 首次等待 30 秒
    silenceTimer_ = 0.0f;
    waitingForNext_ = true;

    initialized_ = true;
    return true;
}

void MusicManager::shutdown() {
    if (!initialized_) return;

    if (currentSound_) {
        if (playing_) {
            ma_sound_stop(currentSound_);
            ma_sound_uninit(currentSound_);
        }
        delete currentSound_;
        currentSound_ = nullptr;
    }

    engine_ = nullptr;
    initialized_ = false;
    playing_ = false;
}

void MusicManager::update(float dt) {
    if (!initialized_ || !enabled_) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // 处理淡入淡出
    updateFade(dt);

    // 检查当前曲目是否播放完毕
    if (playing_ && currentSound_ && !ma_sound_is_playing(currentSound_)) {
        // 曲目结束，进入等待状态
        ma_sound_uninit(currentSound_);
        playing_ = false;
        waitingForNext_ = true;
        silenceTimer_ = 0.0f;
        silenceDuration_ = randomSilenceDuration();
    }

    // 等待间隔后自动播放下一首
    if (waitingForNext_ && !playing_) {
        silenceTimer_ += dt;
        if (silenceTimer_ >= silenceDuration_) {
            playNext();
            waitingForNext_ = false;
        }
    }
}

void MusicManager::setContext(MusicContext ctx) {
    if (ctx == currentContext_) return;

    std::lock_guard<std::mutex> lock(mutex_);

    MusicContext oldCtx = currentContext_;
    currentContext_ = ctx;

    // 如果正在播放，淡出后切换
    if (playing_) {
        fadeState_ = FadeState::FadeOutThenSwitch;
        fadeTimer_ = 0.0f;
    } else {
        // 没在播放，直接开始新场景的等待
        waitingForNext_ = true;
        silenceTimer_ = 0.0f;
        silenceDuration_ = 5.0f; // 切换场景后较快开始播放
    }
}

void MusicManager::play() {
    std::lock_guard<std::mutex> lock(mutex_);
    playNext();
}

void MusicManager::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (playing_) {
        fadeState_ = FadeState::FadingOut;
        fadeTimer_ = 0.0f;
    }
}

void MusicManager::pause() {
    if (playing_ && currentSound_) {
        ma_sound_stop(currentSound_);
        paused_ = true;
    }
}

void MusicManager::resume() {
    if (paused_ && currentSound_) {
        ma_sound_start(currentSound_);
        paused_ = false;
    }
}

void MusicManager::setVolume(float vol) {
    volume_ = std::clamp(vol, 0.0f, 1.0f);
    if (playing_ && currentSound_) {
        ma_sound_set_volume(currentSound_, volume_);
    }
}

bool MusicManager::hasMusic() const {
    for (int i = 0; i < static_cast<int>(MusicContext::Count); ++i) {
        if (!musicFiles_[i].empty()) return true;
    }
    return false;
}

// ========== 内部实现 ==========

void MusicManager::playNext() {
    if (!engine_) return;

    const auto& files = musicFiles_[static_cast<int>(currentContext_)];
    if (files.empty()) return;

    // 停止当前播放
    if (playing_ && currentSound_) {
        ma_sound_stop(currentSound_);
        ma_sound_uninit(currentSound_);
        playing_ = false;
    }

    // 随机选择一首
    std::uniform_int_distribution<size_t> dist(0, files.size() - 1);
    const std::string& file = files[dist(rng_)];

    // 初始化并播放
    ma_result result = ma_sound_init_from_file(engine_, file.c_str(),
                                                MA_SOUND_FLAG_STREAM, // 流式播放，节省内存
                                                nullptr, nullptr, currentSound_);
    if (result != MA_SUCCESS) {
        std::cerr << "[MusicManager] 无法播放: " << file << std::endl;
        return;
    }

    // 2D 播放（背景音乐不需要空间化）
    ma_sound_set_spatialization_enabled(currentSound_, MA_FALSE);
    ma_sound_set_volume(currentSound_, 0.0f); // 从 0 开始淡入

    ma_sound_start(currentSound_);
    playing_ = true;
    paused_ = false;

    // 开始淡入
    fadeState_ = FadeState::FadingIn;
    fadeTimer_ = 0.0f;
}

void MusicManager::updateFade(float dt) {
    if (fadeState_ == FadeState::None) return;
    if (!currentSound_ || !playing_) {
        fadeState_ = FadeState::None;
        return;
    }

    fadeTimer_ += dt;
    float progress = std::clamp(fadeTimer_ / fadeDuration_, 0.0f, 1.0f);

    switch (fadeState_) {
        case FadeState::FadingIn:
            ma_sound_set_volume(currentSound_, volume_ * progress);
            if (progress >= 1.0f) {
                fadeState_ = FadeState::None;
            }
            break;

        case FadeState::FadingOut:
            ma_sound_set_volume(currentSound_, volume_ * (1.0f - progress));
            if (progress >= 1.0f) {
                ma_sound_stop(currentSound_);
                ma_sound_uninit(currentSound_);
                playing_ = false;
                fadeState_ = FadeState::None;
            }
            break;

        case FadeState::FadeOutThenSwitch:
            ma_sound_set_volume(currentSound_, volume_ * (1.0f - progress));
            if (progress >= 1.0f) {
                ma_sound_stop(currentSound_);
                ma_sound_uninit(currentSound_);
                playing_ = false;
                fadeState_ = FadeState::None;
                // 切换后立即开始新场景
                waitingForNext_ = true;
                silenceTimer_ = 0.0f;
                silenceDuration_ = 3.0f;
            }
            break;

        default:
            break;
    }
}

float MusicManager::randomSilenceDuration() {
    // MC 原版：音乐之间间隔 10~20 分钟（600~1200 秒）
    std::uniform_real_distribution<float> dist(600.0f, 1200.0f);
    return dist(rng_);
}
