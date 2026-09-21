#pragma once
#include <string>
#include <vector>
#include <ctime>

namespace mg {
namespace launcher {

struct ProjectInfo {
    std::string name;
    std::string path;
    std::string engine_version;
    std::string mode;           // 2d_topdown / 2d_sidescroller / 3d / empty
    std::string last_opened;
    std::string created_at;
    std::string start_scene;
};

class ProjectManager {
public:
    bool LoadProjects();
    void SaveProjects();
    
    void AddProject(const ProjectInfo& info);
    void RemoveProject(size_t index);
    
    const std::vector<ProjectInfo>& GetProjects() const { return projects_; }
    
    bool CreateProject(const std::string& name, const std::string& path, const std::string& mode);
    bool LoadProjectFromDisk(const std::string& project_path);
    
    // 启动时验证：自动清理已删除的项目，返回是否有项目被移除
    bool ValidateProjects();
    // 重新定位项目路径（项目被移动后手动修复）
    bool RelocateProject(size_t index, const std::string& new_path);
    
private:
    std::vector<ProjectInfo> projects_;
    std::string config_path_ = "launcher/projects.json";
};

} // namespace launcher
} // namespace mg
