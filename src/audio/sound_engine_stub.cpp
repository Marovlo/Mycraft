// ============================================================
// SoundEngine Stub for Headless Dedicated Server
// 服务器不产生声音，但实体/AI 代码会调用 SoundEngine 接口。
// 这里提供空实现以满足链接器，不引入 miniaudio 依赖。
// ============================================================
#include "sound_engine.h"

SoundEngine::SoundEngine() = default;
SoundEngine::~SoundEngine() = default;

bool SoundEngine::init(const std::string& /*soundsBasePath*/) {
    initialized_ = false;
    return true;  // headless server 中静默跳过
}

void SoundEngine::shutdown() {}

void SoundEngine::update() {}

void SoundEngine::play(SoundEventId /*event*/, const glm::vec3& /*position*/,
                       float /*volumeScale*/, float /*pitchScale*/) {}

void SoundEngine::play2D(SoundEventId /*event*/,
                         float /*volumeScale*/, float /*pitchScale*/) {}

void SoundEngine::playBlockBreak(SoundMaterial /*mat*/, const glm::vec3& /*pos*/) {}
void SoundEngine::playBlockPlace(SoundMaterial /*mat*/, const glm::vec3& /*pos*/) {}
void SoundEngine::playStep(SoundMaterial /*mat*/, const glm::vec3& /*pos*/) {}

void SoundEngine::setListenerPosition(const glm::vec3&, const glm::vec3&, const glm::vec3&) {}

void SoundEngine::setMasterVolume(float v) { masterVolume_ = v; }
void SoundEngine::setSFXVolume(float v)    { sfxVolume_    = v; }
void SoundEngine::setMusicVolume(float v)  { musicVolume_  = v; }

// 材质到音效 ID 的映射（与完整实现保持一致，纯查表无副作用）
SoundEventId SoundEngine::getDigEvent(SoundMaterial mat) {
    switch (mat) {
        case SoundMaterial::Stone:    return SoundEventId::DigStone;
        case SoundMaterial::Wood:     return SoundEventId::DigWood;
        case SoundMaterial::Gravel:   return SoundEventId::DigGravel;
        case SoundMaterial::Grass:    return SoundEventId::DigGrass;
        case SoundMaterial::Sand:     return SoundEventId::DigSand;
        case SoundMaterial::Snow:     return SoundEventId::DigSnow;
        case SoundMaterial::Cloth:    return SoundEventId::DigCloth;
        case SoundMaterial::Coral:    return SoundEventId::DigCoral;
        case SoundMaterial::WetGrass: return SoundEventId::DigWetGrass;
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
        case SoundMaterial::Coral:    return SoundEventId::StepCoral;
        case SoundMaterial::WetGrass: return SoundEventId::StepWetGrass;
        default:                      return SoundEventId::StepStone;
    }
}

SoundEngine& getSoundEngine() {
    static SoundEngine instance;
    return instance;
}
