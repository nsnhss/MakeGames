#include "editor/command_history.h"
#include "editor/selection_manager.h"
#include "scene/world.h"

namespace mg {
namespace editor {

// ---- CommandHistory ----

void CommandHistory::Execute(std::unique_ptr<ICommand> cmd) {
    if (cmd) {
        cmd->SetSelectionManager(selection_);
        cmd->Execute();
        undo_stack_.push_back(std::move(cmd));
        if (undo_stack_.size() > kMaxUndoSteps)
            undo_stack_.erase(undo_stack_.begin());
        redo_stack_.clear();
    }
}

void CommandHistory::Undo() {
    if (!undo_stack_.empty()) {
        auto cmd = std::move(undo_stack_.back());
        undo_stack_.pop_back();
        cmd->Undo();
        redo_stack_.push_back(std::move(cmd));
    }
}

void CommandHistory::Redo() {
    if (!redo_stack_.empty()) {
        auto cmd = std::move(redo_stack_.back());
        redo_stack_.pop_back();
        cmd->Execute();
        undo_stack_.push_back(std::move(cmd));
    }
}

// ---- Entity 命令实现 ----
// 撤销/重做后需要同步编辑器选区，选区管理器由 CommandHistory 入栈时注入（无单例依赖）

DeleteEntityCommand::DeleteEntityCommand(scene::World* world, scene::EntityID entity)
    : world_(world), entity_(entity) {}

void DeleteEntityCommand::Execute() {
    if (world_ && world_->IsValid(entity_)) {
        snapshot_ = world_->SnapshotEntity(entity_);
        world_->DestroyEntity(entity_);
        entity_ = 0;
    }
}

void DeleteEntityCommand::Undo() {
    if (world_) {
        entity_ = world_->RestoreEntity(snapshot_);
        if (selection_) selection_->SelectEntity(entity_);
    }
}

CreateEntityCommand::CreateEntityCommand(scene::World* world, scene::EntityID entity)
    : world_(world), entity_(entity) {}

void CreateEntityCommand::Execute() {
    // 首次 Execute 实体已由 UI 层创建；Redo 时从快照恢复
    if (world_ && entity_ == 0) {
        entity_ = world_->RestoreEntity(snapshot_);
        if (selection_) selection_->SelectEntity(entity_);
    }
}

void CreateEntityCommand::Undo() {
    if (world_ && world_->IsValid(entity_)) {
        snapshot_ = world_->SnapshotEntity(entity_);
        world_->DestroyEntity(entity_);
        entity_ = 0;
    }
}

UpdateEntityCommand::UpdateEntityCommand(scene::World* world, scene::EntityID entity,
                                         nlohmann::json after)
    : world_(world), entity_(entity), after_(std::move(after)) {
    if (world_ && world_->IsValid(entity_)) {
        before_ = world_->SnapshotEntity(entity_);
    }
}

void UpdateEntityCommand::Execute() {
    if (world_ && world_->IsValid(entity_)) {
        world_->RestoreEntityInto(entity_, after_);
        if (selection_) selection_->SelectEntity(entity_);
    }
}

void UpdateEntityCommand::Undo() {
    if (world_ && world_->IsValid(entity_)) {
        world_->RestoreEntityInto(entity_, before_);
        if (selection_) selection_->SelectEntity(entity_);
    }
}

} // namespace editor
} // namespace mg
