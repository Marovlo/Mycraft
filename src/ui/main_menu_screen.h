#pragma once

#include "renderer/ui_renderer.h"
#include "renderer/texture_atlas.h"
#include "core/input.h"
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

// 游戏全局状态
enum class GameState {
    MainMenu,       // 主菜单
    WorldSelect,    // 世界选择列表
    CreateWorld,    // 创建新世界
    Playing         // 游戏中
};

// 世界存档信息（从 saves/ 目录扫描得到）
struct WorldInfo {
    std::string name;           // 世界名称
    std::string dirName;        // 目录名（和 name 相同）
    int64_t     seed = 0;       // 种子
    uint64_t    totalTicks = 0; // 总 tick 数
    std::string lastPlayed;     // 最后游玩时间（格式化字符串）
    uint64_t    sizeBytes = 0;  // 存档大小
};

// ============================================================
// 主菜单界面
// ============================================================
class MainMenuScreen {
public:
    void init(UIRenderer* ui, TextureAtlas* atlas);

    // 返回值：用户选择的动作
    enum class Action { None, SinglePlayer, Quit };
    Action update(InputManager& input, float screenW, float screenH);
    void draw(float screenW, float screenH);

private:
    UIRenderer* ui_ = nullptr;
    TextureAtlas* atlas_ = nullptr;

    int hoveredButton_ = -1;  // -1=无, 0=单人游戏, 1=退出

    struct Button {
        std::string label;
        float x, y, w, h;
    };
    std::vector<Button> buttons_;
    void layoutButtons(float screenW, float screenH);
};

// ============================================================
// 世界选择界面
// ============================================================
class WorldSelectScreen {
public:
    void init(UIRenderer* ui, TextureAtlas* atlas);

    enum class Action { None, PlayWorld, CreateNew, DeleteWorld, Back };
    struct Result {
        Action action = Action::None;
        int worldIndex = -1;  // 选中的世界索引（PlayWorld/DeleteWorld 时有效）
    };

    // 扫描 saves/ 目录，刷新世界列表
    void refreshWorldList(const std::string& savesDir);

    Result update(InputManager& input, float screenW, float screenH);
    void draw(float screenW, float screenH);

    const std::vector<WorldInfo>& getWorlds() const { return worlds_; }
    const WorldInfo& getSelectedWorld() const { return worlds_[selectedIndex_]; }

private:
    UIRenderer* ui_ = nullptr;
    TextureAtlas* atlas_ = nullptr;

    std::vector<WorldInfo> worlds_;
    int selectedIndex_ = -1;
    int hoveredIndex_ = -1;
    int hoveredButton_ = -1;  // 0=进入游戏, 1=创建新世界, 2=删除, 3=返回
    int scrollOffset_ = 0;

    // 确认删除对话框
    bool showDeleteConfirm_ = false;
    int deleteTargetIndex_ = -1;
    int deleteHoveredBtn_ = -1;  // 0=确认, 1=取消
};

// ============================================================
// 创建世界界面
// ============================================================
class CreateWorldScreen {
public:
    void init(UIRenderer* ui, TextureAtlas* atlas);

    enum class Action { None, Create, Cancel };
    struct Result {
        Action action = Action::None;
        std::string worldName;
        int64_t seed = 0;
    };

    void open();  // 重置输入状态
    Result update(InputManager& input, float screenW, float screenH);
    void draw(float screenW, float screenH);

private:
    UIRenderer* ui_ = nullptr;
    TextureAtlas* atlas_ = nullptr;

    std::string nameInput_;
    std::string seedInput_;
    int activeField_ = 0;  // 0=名称, 1=种子
    int hoveredButton_ = -1;  // 0=创建, 1=取消

    // 光标闪烁
    double cursorBlinkTime_ = 0.0;
    bool cursorVisible_ = true;
};
