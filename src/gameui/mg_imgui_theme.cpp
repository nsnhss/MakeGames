#include "mg_imgui_theme.h"
#include "imgui.h"
#include "resource/vfs.h"
#include "core/log.h"
#include <stdio.h>
#include <cstring>
#include <vector>

namespace mg {
namespace gameui {

void MG_ApplyTheme_UE5Dark()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // ---- 色彩体系（UE5 暗色） ----
    const ImVec4 UE5_WindowBg         = ImVec4(0.059f, 0.059f, 0.059f, 1.00f);  // #0F0F0F
    const ImVec4 UE5_ChildBg          = ImVec4(0.082f, 0.082f, 0.082f, 1.00f);  // #151515
    [[maybe_unused]] const ImVec4 UE5_PanelBg          = ImVec4(0.102f, 0.102f, 0.102f, 1.00f);  // #1A1A1A
    const ImVec4 UE5_Header           = ImVec4(0.129f, 0.129f, 0.129f, 1.00f);  // #212121
    const ImVec4 UE5_HeaderActive     = ImVec4(0.169f, 0.169f, 0.169f, 1.00f);  // #2B2B2B
    const ImVec4 UE5_HeaderHovered    = ImVec4(0.149f, 0.149f, 0.149f, 1.00f);  // #262626
    const ImVec4 UE5_Border           = ImVec4(0.18f,  0.18f,  0.18f,  1.00f);  // #2E2E2E
    const ImVec4 UE5_BorderShadow     = ImVec4(0.00f,  0.00f,  0.00f,  0.00f);  // 无阴影
    const ImVec4 UE5_Text             = ImVec4(0.88f,  0.88f,  0.88f,  1.00f);  // #E0E0E0
    const ImVec4 UE5_TextDisabled     = ImVec4(0.38f,  0.38f,  0.38f,  1.00f);  // #606060
    const ImVec4 UE5_TextDim          = ImVec4(0.63f,  0.63f,  0.63f,  1.00f);  // #A0A0A0
    const ImVec4 UE5_Accent           = ImVec4(0.169f, 0.396f, 0.890f, 1.00f);  // #2B65E3 UE蓝
    const ImVec4 UE5_AccentHover      = ImVec4(0.220f, 0.470f, 0.950f, 1.00f);  // #3878F2
    [[maybe_unused]] const ImVec4 UE5_AccentActive     = ImVec4(0.125f, 0.345f, 0.820f, 1.00f);  // #2058D1
    const ImVec4 UE5_Button           = ImVec4(0.129f, 0.129f, 0.129f, 1.00f);  // #212121
    const ImVec4 UE5_ButtonHovered    = ImVec4(0.169f, 0.169f, 0.169f, 1.00f);  // #2B2B2B
    const ImVec4 UE5_ButtonActive     = ImVec4(0.200f, 0.200f, 0.200f, 1.00f);  // #333333
    const ImVec4 UE5_FrameBg          = ImVec4(0.102f, 0.102f, 0.102f, 1.00f);  // #1A1A1A
    const ImVec4 UE5_FrameBgHovered   = ImVec4(0.129f, 0.129f, 0.129f, 1.00f);  // #212121
    const ImVec4 UE5_FrameBgActive    = ImVec4(0.149f, 0.149f, 0.149f, 1.00f);  // #262626
    const ImVec4 UE5_TitleBg          = ImVec4(0.059f, 0.059f, 0.059f, 1.00f);  // #0F0F0F
    const ImVec4 UE5_TitleBgActive    = ImVec4(0.082f, 0.082f, 0.082f, 1.00f);  // #151515
    const ImVec4 UE5_CheckMark        = ImVec4(0.169f, 0.396f, 0.890f, 1.00f);  // Accent
    const ImVec4 UE5_SliderGrab       = ImVec4(0.38f,  0.38f,  0.38f,  1.00f);  // #606060
    const ImVec4 UE5_SliderGrabActive = ImVec4(0.50f,  0.50f,  0.50f,  1.00f);  // #808080
    const ImVec4 UE5_ScrollbarBg      = ImVec4(0.059f, 0.059f, 0.059f, 1.00f);  // #0F0F0F
    const ImVec4 UE5_ScrollbarGrab    = ImVec4(0.22f,  0.22f,  0.22f,  1.00f);  // #383838
    const ImVec4 UE5_ScrollbarGrabHovered = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);  // #4D4D4D
    const ImVec4 UE5_ScrollbarGrabActive  = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);  // #606060
    [[maybe_unused]] const ImVec4 UE5_Success          = ImVec4(0.20f,  0.80f,  0.20f,  1.00f);  // 绿
    [[maybe_unused]] const ImVec4 UE5_Warning          = ImVec4(0.95f,  0.65f,  0.10f,  1.00f);  // 橙
    [[maybe_unused]] const ImVec4 UE5_Error            = ImVec4(0.90f,  0.20f,  0.20f,  1.00f);  // 红

    // ---- 全局颜色映射 ----
    colors[ImGuiCol_Text]                   = UE5_Text;
    colors[ImGuiCol_TextDisabled]           = UE5_TextDisabled;
    colors[ImGuiCol_WindowBg]               = UE5_WindowBg;
    colors[ImGuiCol_ChildBg]                = UE5_ChildBg;
    colors[ImGuiCol_PopupBg]                = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_Border]                 = UE5_Border;
    colors[ImGuiCol_BorderShadow]           = UE5_BorderShadow;
    colors[ImGuiCol_FrameBg]                = UE5_FrameBg;
    colors[ImGuiCol_FrameBgHovered]         = UE5_FrameBgHovered;
    colors[ImGuiCol_FrameBgActive]          = UE5_FrameBgActive;
    colors[ImGuiCol_TitleBg]                = UE5_TitleBg;
    colors[ImGuiCol_TitleBgActive]          = UE5_TitleBgActive;
    colors[ImGuiCol_TitleBgCollapsed]       = UE5_TitleBg;
    colors[ImGuiCol_MenuBarBg]              = UE5_Header;
    colors[ImGuiCol_ScrollbarBg]            = UE5_ScrollbarBg;
    colors[ImGuiCol_ScrollbarGrab]          = UE5_ScrollbarGrab;
    colors[ImGuiCol_ScrollbarGrabHovered]   = UE5_ScrollbarGrabHovered;
    colors[ImGuiCol_ScrollbarGrabActive]    = UE5_ScrollbarGrabActive;
    colors[ImGuiCol_CheckMark]              = UE5_CheckMark;
    colors[ImGuiCol_SliderGrab]             = UE5_SliderGrab;
    colors[ImGuiCol_SliderGrabActive]       = UE5_SliderGrabActive;
    colors[ImGuiCol_Button]                 = UE5_Button;
    colors[ImGuiCol_ButtonHovered]          = UE5_ButtonHovered;
    colors[ImGuiCol_ButtonActive]           = UE5_ButtonActive;
    colors[ImGuiCol_Header]                 = UE5_Header;
    colors[ImGuiCol_HeaderHovered]          = UE5_HeaderHovered;
    colors[ImGuiCol_HeaderActive]           = UE5_HeaderActive;
    colors[ImGuiCol_Separator]              = UE5_Border;
    colors[ImGuiCol_SeparatorHovered]       = UE5_TextDim;
    colors[ImGuiCol_SeparatorActive]        = UE5_Accent;
    colors[ImGuiCol_ResizeGrip]             = UE5_SliderGrab;
    colors[ImGuiCol_ResizeGripHovered]      = UE5_SliderGrabActive;
    colors[ImGuiCol_ResizeGripActive]       = UE5_Accent;
    colors[ImGuiCol_Tab]                    = UE5_FrameBg;
    colors[ImGuiCol_TabHovered]             = UE5_HeaderHovered;
    colors[ImGuiCol_TabActive]              = UE5_HeaderActive;
    colors[ImGuiCol_TabUnfocused]           = UE5_FrameBg;
    colors[ImGuiCol_TabUnfocusedActive]     = UE5_Header;
    colors[ImGuiCol_DockingPreview]         = ImVec4(UE5_Accent.x, UE5_Accent.y, UE5_Accent.z, 0.30f);
    colors[ImGuiCol_DockingEmptyBg]         = UE5_WindowBg;
    colors[ImGuiCol_PlotLines]              = UE5_TextDim;
    colors[ImGuiCol_PlotLinesHovered]       = UE5_Accent;
    colors[ImGuiCol_PlotHistogram]          = UE5_Accent;
    colors[ImGuiCol_PlotHistogramHovered]   = UE5_AccentHover;
    colors[ImGuiCol_TableHeaderBg]          = UE5_Header;
    colors[ImGuiCol_TableBorderStrong]      = UE5_Border;
    colors[ImGuiCol_TableBorderLight]       = ImVec4(UE5_Border.x, UE5_Border.y, UE5_Border.z, 0.50f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(UE5_Accent.x, UE5_Accent.y, UE5_Accent.z, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = UE5_Accent;
    colors[ImGuiCol_NavHighlight]           = UE5_Accent;
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.35f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);

    // ---- 尺寸与圆角 ----
    style.WindowRounding     = 0.0f;
    style.ChildRounding      = 2.0f;
    style.FrameRounding      = 3.0f;
    style.PopupRounding      = 4.0f;
    style.ScrollbarRounding  = 4.0f;
    style.GrabRounding       = 3.0f;
    style.TabRounding        = 2.0f;
    style.WindowBorderSize   = 1.0f;
    style.FrameBorderSize    = 1.0f;
    style.PopupBorderSize    = 1.0f;
    style.WindowPadding      = ImVec2(8, 8);
    style.FramePadding       = ImVec2(6, 4);
    style.ItemSpacing        = ImVec2(6, 4);
    style.ItemInnerSpacing   = ImVec2(6, 4);
    style.CellPadding        = ImVec2(4, 2);
    style.ScrollbarSize      = 10.0f;
    style.GrabMinSize        = 10.0f;
}

ImFont* MG_LoadFonts(float scale)
{
    ImGuiIO& io = ImGui::GetIO();

    // ---- 主字体：中文回退链 msyh（微软雅黑）→ simhei（黑体）→ 默认字体 ----
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 2;
    font_cfg.OversampleV = 2;
    font_cfg.PixelSnapH = true;

    const char* cjk_candidates[] = {
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/simsun.ttc",
    };
    ImFont* font = nullptr;
    for (const char* path : cjk_candidates) {
        font = io.Fonts->AddFontFromFileTTF(
            path,
            11.0f * scale,
            &font_cfg,
            io.Fonts->GetGlyphRangesChineseSimplifiedCommon()
        );
        if (font) break;
        MG_LOG_WARN("MG_LoadFonts: CJK 字体加载失败，尝试下一个: " << path);
    }

    if (font == nullptr)
    {
        // 降级：使用默认字体（中文将显示为豆腐块，需用户可见警告）
        MG_LOG_WARN("MG_LoadFonts: 所有 CJK 字体均不可用，回退默认字体（中文将丢失）");
        font = io.Fonts->AddFontDefault();
    }

    // ---- 图标字体：FontAwesome 6 Free Solid 14px（合并模式） ----
    static const ImWchar icon_ranges[] = { 0xE000, 0xF8FF, 0 };
    ImFontConfig icons_cfg;
    icons_cfg.MergeMode = true;
    icons_cfg.PixelSnapH = true;
    icons_cfg.OversampleH = 1;
    icons_cfg.OversampleV = 1;
    icons_cfg.GlyphOffset.y = 1.0f * scale;
    icons_cfg.GlyphMinAdvanceX = 14.0f * scale;

    // ---- 图标字体：FontAwesome 6 Free Solid 14px（合并模式） ----
    // 经 VFS 读取（pak 命中读包），内存加载；缓冲区所有权移交 ImGui 字体图集
    std::vector<uint8_t> icon_bytes;
    if (vfs::ReadFile("assets/fonts/fa-solid-900.ttf", icon_bytes)) {
        void* icon_mem = ImGui::MemAlloc(icon_bytes.size());
        memcpy(icon_mem, icon_bytes.data(), icon_bytes.size());
        ImFont* icons = io.Fonts->AddFontFromMemoryTTF(
            icon_mem, (int)icon_bytes.size(),
            10.0f * scale,
            &icons_cfg,
            icon_ranges
        );
        // 注意：缓冲区所有权已移交 ImGui（即使失败也由 atlas 统一释放，不可手动 free）
        if (!icons) {
            MG_LOG_WARN("MG_LoadFonts: FontAwesome 图标字体解析失败（图标将显示为豆腐块）");
        }
    } else {
        MG_LOG_WARN("MG_LoadFonts: 图标字体缺失 assets/fonts/fa-solid-900.ttf（图标将显示为豆腐块）");
    }

    return font;
}

void MG_ApplyTheme_UE5Light()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // ---- 色彩体系（UE5 亮色） ----
    const ImVec4 UE5_WindowBg         = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);  // #F5F5F5
    const ImVec4 UE5_ChildBg          = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);  // #EFEFEF
    [[maybe_unused]] const ImVec4 UE5_PanelBg          = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);  // #EBEBEB
    const ImVec4 UE5_Header           = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);  // #E6E6E6
    const ImVec4 UE5_HeaderActive     = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);  // #DBDBDB
    const ImVec4 UE5_HeaderHovered    = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);  // #E0E0E0
    const ImVec4 UE5_Border           = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);  // #CCCCCC
    const ImVec4 UE5_Text             = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);  // #262626
    const ImVec4 UE5_TextDisabled     = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);  // #8C8C8C
    const ImVec4 UE5_TextDim          = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);  // #666666
    const ImVec4 UE5_Accent           = ImVec4(0.169f, 0.396f, 0.890f, 1.00f);  // #2B65E3
    const ImVec4 UE5_AccentHover      = ImVec4(0.220f, 0.470f, 0.950f, 1.00f);  // #3878F2
    [[maybe_unused]] const ImVec4 UE5_AccentActive     = ImVec4(0.125f, 0.345f, 0.820f, 1.00f);  // #2058D1
    const ImVec4 UE5_Button           = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);  // #E0E0E0
    const ImVec4 UE5_ButtonHovered    = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);  // #D1D1D1
    const ImVec4 UE5_ButtonActive     = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);  // #C2C2C2
    const ImVec4 UE5_FrameBg          = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);  // #FFFFFF
    const ImVec4 UE5_FrameBgHovered   = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);  // #F5F5F5
    const ImVec4 UE5_FrameBgActive    = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);  // #EBEBEB
    const ImVec4 UE5_TitleBg          = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);  // #EFEFEF
    const ImVec4 UE5_TitleBgActive    = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);  // #E6E6E6
    const ImVec4 UE5_CheckMark        = UE5_Accent;
    const ImVec4 UE5_SliderGrab       = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    const ImVec4 UE5_SliderGrabActive = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    const ImVec4 UE5_ScrollbarBg      = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    const ImVec4 UE5_ScrollbarGrab    = ImVec4(0.72f, 0.72f, 0.72f, 1.00f);
    const ImVec4 UE5_ScrollbarGrabHovered = ImVec4(0.64f, 0.64f, 0.64f, 1.00f);
    const ImVec4 UE5_ScrollbarGrabActive  = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);

    colors[ImGuiCol_Text]                   = UE5_Text;
    colors[ImGuiCol_TextDisabled]           = UE5_TextDisabled;
    colors[ImGuiCol_WindowBg]               = UE5_WindowBg;
    colors[ImGuiCol_ChildBg]                = UE5_ChildBg;
    colors[ImGuiCol_PopupBg]                = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
    colors[ImGuiCol_Border]                 = UE5_Border;
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = UE5_FrameBg;
    colors[ImGuiCol_FrameBgHovered]         = UE5_FrameBgHovered;
    colors[ImGuiCol_FrameBgActive]          = UE5_FrameBgActive;
    colors[ImGuiCol_TitleBg]                = UE5_TitleBg;
    colors[ImGuiCol_TitleBgActive]          = UE5_TitleBgActive;
    colors[ImGuiCol_TitleBgCollapsed]       = UE5_TitleBg;
    colors[ImGuiCol_MenuBarBg]              = UE5_Header;
    colors[ImGuiCol_ScrollbarBg]            = UE5_ScrollbarBg;
    colors[ImGuiCol_ScrollbarGrab]          = UE5_ScrollbarGrab;
    colors[ImGuiCol_ScrollbarGrabHovered]   = UE5_ScrollbarGrabHovered;
    colors[ImGuiCol_ScrollbarGrabActive]    = UE5_ScrollbarGrabActive;
    colors[ImGuiCol_CheckMark]              = UE5_CheckMark;
    colors[ImGuiCol_SliderGrab]             = UE5_SliderGrab;
    colors[ImGuiCol_SliderGrabActive]       = UE5_SliderGrabActive;
    colors[ImGuiCol_Button]                 = UE5_Button;
    colors[ImGuiCol_ButtonHovered]          = UE5_ButtonHovered;
    colors[ImGuiCol_ButtonActive]           = UE5_ButtonActive;
    colors[ImGuiCol_Header]                 = UE5_Header;
    colors[ImGuiCol_HeaderHovered]          = UE5_HeaderHovered;
    colors[ImGuiCol_HeaderActive]           = UE5_HeaderActive;
    colors[ImGuiCol_Separator]              = UE5_Border;
    colors[ImGuiCol_SeparatorHovered]       = UE5_TextDim;
    colors[ImGuiCol_SeparatorActive]        = UE5_Accent;
    colors[ImGuiCol_ResizeGrip]             = UE5_SliderGrab;
    colors[ImGuiCol_ResizeGripHovered]      = UE5_SliderGrabActive;
    colors[ImGuiCol_ResizeGripActive]       = UE5_Accent;
    colors[ImGuiCol_Tab]                    = UE5_FrameBg;
    colors[ImGuiCol_TabHovered]             = UE5_HeaderHovered;
    colors[ImGuiCol_TabActive]              = UE5_HeaderActive;
    colors[ImGuiCol_TabUnfocused]           = UE5_FrameBg;
    colors[ImGuiCol_TabUnfocusedActive]     = UE5_Header;
    colors[ImGuiCol_DockingPreview]         = ImVec4(UE5_Accent.x, UE5_Accent.y, UE5_Accent.z, 0.25f);
    colors[ImGuiCol_DockingEmptyBg]         = UE5_WindowBg;
    colors[ImGuiCol_PlotLines]              = UE5_TextDim;
    colors[ImGuiCol_PlotLinesHovered]       = UE5_Accent;
    colors[ImGuiCol_PlotHistogram]          = UE5_Accent;
    colors[ImGuiCol_PlotHistogramHovered]   = UE5_AccentHover;
    colors[ImGuiCol_TableHeaderBg]          = UE5_Header;
    colors[ImGuiCol_TableBorderStrong]      = UE5_Border;
    colors[ImGuiCol_TableBorderLight]       = ImVec4(UE5_Border.x, UE5_Border.y, UE5_Border.z, 0.50f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(0.00f, 0.00f, 0.00f, 0.03f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(UE5_Accent.x, UE5_Accent.y, UE5_Accent.z, 0.25f);
    colors[ImGuiCol_DragDropTarget]         = UE5_Accent;
    colors[ImGuiCol_NavHighlight]           = UE5_Accent;
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.20f, 0.20f, 0.20f, 0.25f);

    // 尺寸与圆角同暗色主题
    style.WindowRounding     = 0.0f;
    style.ChildRounding      = 2.0f;
    style.FrameRounding      = 3.0f;
    style.PopupRounding      = 4.0f;
    style.ScrollbarRounding  = 4.0f;
    style.GrabRounding       = 3.0f;
    style.TabRounding        = 2.0f;
    style.WindowBorderSize   = 1.0f;
    style.FrameBorderSize    = 1.0f;
    style.PopupBorderSize    = 1.0f;
    style.WindowPadding      = ImVec2(8, 8);
    style.FramePadding       = ImVec2(6, 4);
    style.ItemSpacing        = ImVec2(6, 4);
    style.ItemInnerSpacing   = ImVec2(6, 4);
    style.CellPadding        = ImVec2(4, 2);
    style.ScrollbarSize      = 10.0f;
    style.GrabMinSize        = 10.0f;
}

} // namespace gameui
} // namespace mg
