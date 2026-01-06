#include "imgui.h"
#include "imgui_config.h"

namespace ImGuiStyleConfig {
    void ApplyDarkTheme() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors    = style.Colors;

        style.WindowRounding    = 6.0f;
        style.FrameRounding     = 4.0f;
        style.ScrollbarRounding = 6.0f;

        colors[ImGuiCol_WindowBg]       = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_Header]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_HeaderHovered]  = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_Button]         = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
        colors[ImGuiCol_ButtonHovered]  = ImVec4(0.30f, 0.35f, 0.40f, 1.00f);
        colors[ImGuiCol_ButtonActive]   = ImVec4(0.15f, 0.20f, 0.25f, 1.00f);
    }
}
