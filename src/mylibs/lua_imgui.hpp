#pragma once
/**
 * @brief ImGui viewer for LuaAPI::history(): every Lua chunk that ran (in-game console,
 * remote console, setup scripts) with its output. Depends only on lua_api.hpp and ImGui,
 * so the Lua core itself stays UI-free.
 *
 * Usage: keep a bool in the game, flip it from a dev-menu button, and call
 * LuaImGui::draw_history(&show) once per frame inside rlImGuiBegin()/rlImGuiEnd().
 */

#include "imgui.h"
#include "lua_api.hpp"

namespace LuaImGui {

/// Draw the "Lua History" window when *open is true. Each entry shows the chunk and what
/// it produced; "Run again" re-evaluates it, so output also lands wherever LuaAPI::log goes.
inline void draw_history(bool* open) {
  if (!open || !*open)
    return;

  ImGui::SetNextWindowSize(ImVec2(560, 480), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Lua History", open)) {
    auto& hist = LuaAPI::history();
    ImGui::Text("%zu chunk(s) run", hist.size());
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear"))
      hist.clear();
    ImGui::SameLine();
    static bool show_output = true;
    ImGui::Checkbox("Show output", &show_output);
    ImGui::Separator();

    // leave room below the list for the editor and its button row
    static float editor_height = 120.0f;
    float footer = editor_height + ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 3;
    ImGui::BeginChild("LuaHistoryList", ImVec2(0, -footer), false, ImGuiWindowFlags_HorizontalScrollbar);
    static size_t last_size = 0;
    std::string rerun; // deferred: re-running mutates hist while we iterate it
    for (size_t i = 0; i < hist.size(); i++) {
      const auto& e = hist[i];
      ImGui::PushID((int)i);
      if (ImGui::SmallButton("Run again"))
        rerun = e.code;
      ImGui::SameLine();
      ImVec4 col = e.ok ? ImVec4(0.6f, 1.0f, 0.6f, 1.0f) : ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
      ImGui::TextColored(col, "[%zu] > %s", i, e.code.c_str());
      if (show_output && !e.output.empty()) {
        ImGui::Indent();
        ImGui::TextUnformatted(e.output.c_str());
        ImGui::Unindent();
      }
      ImGui::PopID();
    }
    if (hist.size() != last_size) {
      ImGui::SetScrollHereY(1.0f);
      last_size = hist.size();
    }
    ImGui::EndChild();
    if (!rerun.empty())
      LuaAPI::eval(rerun);

    // ---- editor: write a chunk here and run it; the result appears in the list above ----
    ImGui::Separator();
    static char editor[8192] = "";
    ImGui::InputTextMultiline("##lua_editor", editor, sizeof(editor), ImVec2(-1, editor_height),
                              ImGuiInputTextFlags_AllowTabInput);
    bool editor_focused = ImGui::IsItemActive();
    bool run = ImGui::Button("Run (Ctrl+Enter)");
    if (editor_focused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter))
      run = true;
    ImGui::SameLine();
    if (ImGui::Button("Clear editor"))
      editor[0] = '\0';
    ImGui::SameLine();
    ImGui::TextDisabled("Lua chunk; a bare expression shows its value");
    if (run && editor[0] != '\0')
      LuaAPI::eval(editor);
  }
  ImGui::End();
}

} // namespace LuaImGui
