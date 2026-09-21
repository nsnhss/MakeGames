#include "pending_editor.h"

namespace mg {
namespace launcher {

namespace {
std::string g_pending_editor_path;
}

void SetPendingEditorPath(const std::string& path) {
    g_pending_editor_path = path;
}

const std::string& GetPendingEditorPath() {
    return g_pending_editor_path;
}

} // namespace launcher
} // namespace mg
