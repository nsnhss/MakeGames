#pragma once
#include <string>
#include <unordered_map>

namespace mg {
namespace scene {

// 内置动作命令类型
enum class ActionCmd {
    ShowMessage,      // 显示文字消息（对话框，阻塞）
    ShowToast,        // 浮动轻提示，不阻塞
    ShowChoices,      // 显示选项
    TransferPlayer,   // 传送玩家到指定地图/坐标
    MoveEntity,       // 移动实体
    PlayAnimation,    // 播放动画
    PlaySound,        // 播放音效
    PlayBGM,          // 切换背景音乐（float_value = 淡入秒数）
    StopBGM,          // 停止背景音乐（float_value = 淡出秒数）
    PlayBGS,          // 播放背景环境音（循环，与 BGM 并行；float_value = 淡入秒数）
    StopBGS,          // 停止背景环境音（float_value = 淡出秒数）
    PlayME,           // 播放音乐效果（一次性，期间 BGM 挂起，结束自动恢复）
    SetVariable,      // 设置全局变量
    SetSwitch,        // 设置全局开关
    SetSelfSwitch,    // 设置事件自身开关
    AddItem,          // 添加物品
    RemoveItem,       // 移除物品
    ShopProcessing,   // 商店处理（打开商店界面）
    BattleProcessing, // 战斗处理（进入回合制战斗）
    ShowUI,           // 显示 UI 面板
    HideUI,           // 隐藏 UI 面板
    Wait,             // 等待 N 秒
    If,               // 条件分支开始
    Else,             // 条件分支否则
    EndIf,            // 条件分支结束
    Loop,             // 循环开始
    BreakLoop,        // 跳出循环
    CallScript,       // 调用 Lua 脚本
    Unknown
};

// 单条动作命令
struct Action {
    ActionCmd cmd = ActionCmd::Unknown;
    // 通用参数字段（按需使用）
    std::string text;       // 文字内容/脚本路径
    std::string target;     // 目标实体/变量名
    std::string param;      // 通用字符串参数
    int int_value = 0;      // 整型参数
    float float_value = 0.0f; // 浮点参数
    bool bool_value = false;  // 布尔参数
};

} // namespace scene
} // namespace mg
