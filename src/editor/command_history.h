#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "scene/entity.h"
#include "third_party/json.hpp"

namespace mg {
namespace scene { class World; }
namespace editor {

class SelectionManager;

// Undo/Redo 命令基类
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    virtual const char* GetName() const = 0;
    // 由 CommandHistory 在入栈时注入选区管理器（Undo/Redo 后同步编辑器选区）
    virtual void SetSelectionManager(SelectionManager*) {}
};

class DeleteEntityCommand : public ICommand {
public:
    DeleteEntityCommand(scene::World* world, scene::EntityID entity);
    void Execute() override;
    void Undo() override;
    const char* GetName() const override { return "Delete Entity"; }
    void SetSelectionManager(SelectionManager* sel) override { selection_ = sel; }
private:
    scene::World* world_ = nullptr;
    scene::EntityID entity_ = 0;
    SelectionManager* selection_ = nullptr;
    nlohmann::json snapshot_;
};

class CreateEntityCommand : public ICommand {
public:
    CreateEntityCommand(scene::World* world, scene::EntityID entity);
    void Execute() override;
    void Undo() override;
    const char* GetName() const override { return "Create Entity"; }
    void SetSelectionManager(SelectionManager* sel) override { selection_ = sel; }
private:
    scene::World* world_ = nullptr;
    scene::EntityID entity_ = 0;
    SelectionManager* selection_ = nullptr;
    nlohmann::json snapshot_;
};

// 原地更新实体组件（Prefab Revert 等）：构造时捕获 before 快照，after 快照由调用方提供
class UpdateEntityCommand : public ICommand {
public:
    // after = 目标状态快照（调用方先算好，如 prefab 文件内容+保留位置）
    UpdateEntityCommand(scene::World* world, scene::EntityID entity, nlohmann::json after);
    // before/after 均为调用方提供的快照（Details 面板组件编辑：before 在编辑前捕获）
    UpdateEntityCommand(scene::World* world, scene::EntityID entity,
                        nlohmann::json before, nlohmann::json after);
    void Execute() override;
    void Undo() override;
    const char* GetName() const override { return "Update Entity"; }
    void SetSelectionManager(SelectionManager* sel) override { selection_ = sel; }
private:
    scene::World* world_ = nullptr;
    scene::EntityID entity_ = 0;
    SelectionManager* selection_ = nullptr;
    nlohmann::json before_;
    nlohmann::json after_;
};

// 命令历史：Undo/Redo 双栈，容量上限 kMaxUndoSteps
class CommandHistory {
public:
    // 注入选区管理器（EditorContext 构造时调用一次）
    void Bind(SelectionManager* sel) { selection_ = sel; }
    // 执行命令并压入 Undo 栈，同时清空 Redo 栈
    void Execute(std::unique_ptr<ICommand> cmd);
    bool CanUndo() const { return !undo_stack_.empty(); }
    bool CanRedo() const { return !redo_stack_.empty(); }
    void Undo();
    void Redo();
    // 清空两个栈（场景切换/加载新场景时调用，历史不跨场景保留）
    void Clear() { undo_stack_.clear(); redo_stack_.clear(); }

private:
    std::vector<std::unique_ptr<ICommand>> undo_stack_;
    std::vector<std::unique_ptr<ICommand>> redo_stack_;
    SelectionManager* selection_ = nullptr;
    static constexpr size_t kMaxUndoSteps = 128;
};

} // namespace editor
} // namespace mg
