#pragma once

namespace mg {
namespace editor {

class EditorContext;

// Editor 工具栏：播放控制（Play/Pause/Stop）、Undo/Redo、变换工具（T/R/S）、视图模式（Lit/Unlit/Wire）
class EditorToolBar {
public:
    void Draw(EditorContext& ctx);
};

} // namespace editor
} // namespace mg
