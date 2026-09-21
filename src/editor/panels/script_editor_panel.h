#pragma once
#include "editor/panel.h"
#include <functional>
#include <string>

namespace mg {
namespace scene { class EventSystem; }
namespace editor {

class SceneSession;

class ScriptEditorPanel : public IPanel {
public:
    ScriptEditorPanel(SceneSession& session, std::function<scene::EventSystem*()> event_system_getter);
    const char* GetName() const override { return "Script Editor"; }
    const char* GetIcon() const override;
    bool IsPopup() const override { return true; }
    void Draw() override;
    void OpenFile(const std::string& path);

private:
    void LoadFile(const std::string& path);
    void SaveFile();

    SceneSession& session_;
    std::function<scene::EventSystem*()> get_event_system_;

    std::string current_path_;
    std::string buffer_;
    bool dirty_ = false;
    bool focus_text_ = false;
};

} // namespace editor
} // namespace mg
