#pragma once
#include "src/editor/panel.h"

namespace mg {
namespace editor {

class TilesetEditor;
class SceneSession;
class EditorProjectManager;

class TilePalettePanel : public IPanel {
public:
    TilePalettePanel(TilesetEditor& tilesets, SceneSession& session, EditorProjectManager& project)
        : tilesets_(tilesets), session_(session), project_(project) {}
    const char* GetName() const override { return "Tile Palette"; }
    const char* GetIcon() const override;
    bool IsPopup() const override { return true; }
    void Draw() override;

private:
    TilesetEditor& tilesets_;
    SceneSession& session_;
    EditorProjectManager& project_;
    // New Tileset popup state
    char nt_name_[64] = {};
    char nt_path_[256] = {};
    int nt_tile_w_ = 32;
    int nt_tile_h_ = 32;
    int nt_columns_ = 8;
    int nt_rows_ = 4;
    char nt_error_[128] = {};
};

} // namespace editor
} // namespace mg
