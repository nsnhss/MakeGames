#pragma once
#include <memory>
#include <string>
#include <vector>
#include "resource/schema/project_schema.h"

namespace mg {
namespace editor {

// 编辑器侧项目管理：项目路径与清单（ProjectManifest）、新建项目、
// MapInfos.json 重建/读取、最近打开项目列表。
// 打开/保存项目的编排（场景加载、数据库加载）由 EditorContext 门面完成。
class EditorProjectManager {
public:
    void SetProjectPath(const std::string& path) { project_path_ = path; }
    const std::string& GetProjectPath() const { return project_path_; }

    void SetManifest(std::shared_ptr<const ProjectManifest> m) { manifest_ = std::move(m); }
    const ProjectManifest* GetManifest() const { return manifest_.get(); }
    std::shared_ptr<const ProjectManifest> GetManifestShared() const { return manifest_; }

    // 新建项目：创建目录 + project.json（不动编辑器当前状态）
    bool NewProject(const std::string& name, const std::string& path, const std::string& mode);

    // 扫描 maps/ 重建 MapInfos.json（自然排序：Map2 < Map10）
    void RebuildMapInfos();
    // 读取 maps/MapInfos.json
    MapInfos LoadMapInfos() const;

    // 最近打开项目（存 %APPDATA%/MakeGames/recent_projects.json，最多 5 条）
    std::vector<std::string> GetRecentProjects() const;
    void AddRecentProject(const std::string& path);

private:
    std::string project_path_;
    std::shared_ptr<const ProjectManifest> manifest_;
};

} // namespace editor
} // namespace mg
