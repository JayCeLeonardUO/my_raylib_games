/// bin/bash
/*
cd "$(dirname "$0")/../../../" && ./build_run.sh imgui_prototype
exit
*/

#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"
#include <functional>
#include <string>
#include <vector>

// =============================================================================
// Layout API
// =============================================================================

struct ToolbarEntry {
  std::string name;
  bool active = false;
  std::function<void()> onContent = nullptr; // Content to render in right panel when active
};

struct CollapsiblePanel {
  bool isOpen = false;
  bool isLocked = false;
  float size = 200.0f;   // width for side panels, height for bottom
  float tabSize = 20.0f; // collapsed tab size
  std::string title = "Panel";
};

class EditorLayout {
public:
  // Toolbar
  std::vector<ToolbarEntry> toolbarEntries;

  // Panels
  CollapsiblePanel rightPanel = {false, false, 200.0f, 20.0f, "Properties"};
  CollapsiblePanel bottomPanel = {false, false, 150.0f, 20.0f, "Console"};

  // Console output
  std::vector<std::string> consoleLines;

  // Add a toolbar button with optional widget content
  void addToolbarButton(const std::string& name, std::function<void()> contentFn = nullptr) {
    toolbarEntries.push_back({name, false, contentFn});
  }

  // Log to console
  void log(const std::string& msg) { consoleLines.push_back(msg); }

  // Render the full layout
  void render() {
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 mousePos = ImGui::GetIO().MousePos;

    // Calculate toolbar height
    float scrollbarHeight = ImGui::GetStyle().ScrollbarSize;
    float toolbarHeight = ImGui::GetFrameHeightWithSpacing() +
                          ImGui::GetStyle().WindowPadding.y * 2 + scrollbarHeight;

    renderToolbar(displaySize, toolbarHeight);
    renderRightPanel(displaySize, mousePos, toolbarHeight);

    float rightPanelWidth = rightPanel.isOpen ? rightPanel.size : rightPanel.tabSize;
    renderBottomPanel(displaySize, mousePos, toolbarHeight, rightPanelWidth);
  }

private:
  void renderToolbar(ImVec2 displaySize, float toolbarHeight) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(displaySize.x, toolbarHeight));
    ImGui::Begin("Toolbar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::BeginChild("ToolbarScroll", ImVec2(0, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Horizontal scroll with mouse wheel
    if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0) {
      ImGui::SetScrollX(ImGui::GetScrollX() - ImGui::GetIO().MouseWheel * 30.0f);
    }

    for (size_t i = 0; i < toolbarEntries.size(); i++) {
      if (i > 0)
        ImGui::SameLine();

      auto& entry = toolbarEntries[i];

      if (entry.active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
      }

      if (ImGui::Button(entry.name.c_str())) {
        entry.active = !entry.active;
      }

      if (entry.active) {
        ImGui::PopStyleColor();
      }
    }

    ImGui::EndChild();
    ImGui::End();
  }

  void renderRightPanel(ImVec2 displaySize, ImVec2 mousePos, float toolbarHeight) {
    ImVec2 tabPos = ImVec2(displaySize.x - rightPanel.tabSize, toolbarHeight);
    ImVec2 panelPos = ImVec2(displaySize.x - rightPanel.size, toolbarHeight);
    float panelHeight = displaySize.y - toolbarHeight;

    bool hoveringTab = mousePos.x >= tabPos.x && mousePos.y >= toolbarHeight;
    bool hoveringPanel = mousePos.x >= panelPos.x && mousePos.y >= toolbarHeight;

    // State logic
    if (hoveringTab && !rightPanel.isOpen) {
      rightPanel.isOpen = true;
    } else if (rightPanel.isOpen && !hoveringPanel && !rightPanel.isLocked) {
      rightPanel.isOpen = false;
    }

    if (rightPanel.isOpen) {
      ImGui::SetNextWindowPos(panelPos);
      ImGui::SetNextWindowSize(ImVec2(rightPanel.size, panelHeight));
      ImGui::Begin("RightPanel", nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

      // Lock button
      ImGui::PushStyleColor(ImGuiCol_Button, rightPanel.isLocked ? ImVec4(0.2f, 0.6f, 0.2f, 1.0f)
                                                                 : ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
      if (ImGui::Button(rightPanel.isLocked ? "Locked" : "Lock", ImVec2(60, 0))) {
        rightPanel.isLocked = !rightPanel.isLocked;
      }
      ImGui::PopStyleColor();
      ImGui::SameLine();
      ImGui::Text("%s", rightPanel.title.c_str());
      ImGui::Separator();

      // Scrollable content
      ImGui::BeginChild("RightPanelScroll", ImVec2(0, 0), false);

      bool hasActiveWidgets = false;
      for (size_t i = 0; i < toolbarEntries.size(); i++) {
        auto& entry = toolbarEntries[i];
        if (!entry.active)
          continue;
        hasActiveWidgets = true;

        ImGui::PushID((int)i);
        if (ImGui::CollapsingHeader(entry.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
          if (entry.onContent) {
            entry.onContent();
          } else {
            ImGui::Text("Widget: %s", entry.name.c_str());
          }
        }
        ImGui::PopID();
      }

      if (!hasActiveWidgets) {
        ImGui::TextDisabled("Click toolbar buttons to add widgets");
      }

      ImGui::EndChild();
      ImGui::End();
    } else {
      // Collapsed tab
      ImGui::SetNextWindowPos(tabPos);
      ImGui::SetNextWindowSize(ImVec2(rightPanel.tabSize, panelHeight));
      ImGui::Begin("RightTab", nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                       ImGuiWindowFlags_NoScrollbar);

      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
      ImGui::Button(">\n>\n>", ImVec2(rightPanel.tabSize - 8, 60));
      ImGui::PopStyleColor();

      ImGui::End();
    }
  }

  void renderBottomPanel(ImVec2 displaySize, ImVec2 mousePos, float toolbarHeight,
                         float rightPanelWidth) {
    float availWidth = displaySize.x - rightPanelWidth;

    ImVec2 tabPos = ImVec2(0, displaySize.y - bottomPanel.tabSize);
    ImVec2 panelPos = ImVec2(0, displaySize.y - bottomPanel.size);

    bool hoveringTab = mousePos.x < availWidth && mousePos.y >= tabPos.y;
    bool hoveringPanel = mousePos.x < availWidth && mousePos.y >= panelPos.y;

    // State logic
    if (hoveringTab && !bottomPanel.isOpen) {
      bottomPanel.isOpen = true;
    } else if (bottomPanel.isOpen && !hoveringPanel && !bottomPanel.isLocked) {
      bottomPanel.isOpen = false;
    }

    if (bottomPanel.isOpen) {
      ImGui::SetNextWindowPos(panelPos);
      ImGui::SetNextWindowSize(ImVec2(availWidth, bottomPanel.size));
      ImGui::Begin("BottomPanel", nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

      // Lock button
      ImGui::PushStyleColor(ImGuiCol_Button, bottomPanel.isLocked ? ImVec4(0.2f, 0.6f, 0.2f, 1.0f)
                                                                  : ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
      if (ImGui::Button(bottomPanel.isLocked ? "Locked" : "Lock", ImVec2(60, 0))) {
        bottomPanel.isLocked = !bottomPanel.isLocked;
      }
      ImGui::PopStyleColor();
      ImGui::SameLine();
      ImGui::Text("%s", bottomPanel.title.c_str());
      ImGui::Separator();

      // Console output
      ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), false);
      for (const auto& line : consoleLines) {
        ImGui::Text("> %s", line.c_str());
      }
      if (consoleLines.empty()) {
        ImGui::TextDisabled("No console output");
      }
      ImGui::EndChild();

      ImGui::End();
    } else {
      // Collapsed tab
      ImGui::SetNextWindowPos(tabPos);
      ImGui::SetNextWindowSize(ImVec2(availWidth, bottomPanel.tabSize));
      ImGui::Begin("BottomTab", nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                       ImGuiWindowFlags_NoScrollbar);

      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
      ImGui::Button("^ Console ^", ImVec2(100, bottomPanel.tabSize - 4));
      ImGui::PopStyleColor();

      ImGui::End();
    }
  }
};

// =============================================================================
// Main - Example Usage
// =============================================================================

// Demo state for widgets (global for lambda capture)
float demoFloat = 0.5f;
int demoInt = 10;
bool demoBool = true;
float demoColor[3] = {1.0f, 0.5f, 0.2f};
char demoText[128] = "Hello";
int demoCombo = 0;
float demoVec3[3] = {0.0f, 1.0f, 0.0f};

int main() {
  InitWindow(1280, 720, "ImGui Prototype");
  SetTargetFPS(60);
  rlImGuiSetup(true);

  // Create layout
  EditorLayout layout;

  // Add toolbar buttons with custom widget content
  layout.addToolbarButton("File", []() {
    ImGui::Text("Recent Files:");
    ImGui::BulletText("project.cpp");
    ImGui::BulletText("main.hpp");
    ImGui::BulletText("scene.json");
    ImGui::Separator();
    if (ImGui::Button("New", ImVec2(-1, 0))) {}
    if (ImGui::Button("Open", ImVec2(-1, 0))) {}
    if (ImGui::Button("Save", ImVec2(-1, 0))) {}
  });

  layout.addToolbarButton("Edit", []() {
    if (ImGui::Button("Undo", ImVec2(-1, 0))) {}
    if (ImGui::Button("Redo", ImVec2(-1, 0))) {}
    ImGui::Separator();
    if (ImGui::Button("Cut", ImVec2(-1, 0))) {}
    if (ImGui::Button("Copy", ImVec2(-1, 0))) {}
    if (ImGui::Button("Paste", ImVec2(-1, 0))) {}
  });

  layout.addToolbarButton("View", []() {
    ImGui::Checkbox("Grid", &demoBool);
    ImGui::Checkbox("Wireframe", &demoBool);
    ImGui::Checkbox("Normals", &demoBool);
    ImGui::Separator();
    ImGui::SliderFloat("Zoom", &demoFloat, 0.1f, 10.0f);
  });

  layout.addToolbarButton("Select", []() {
    const char* modes[] = {"Box", "Circle", "Lasso", "Paint"};
    ImGui::Combo("Mode", &demoCombo, modes, IM_ARRAYSIZE(modes));
    ImGui::Separator();
    ImGui::Text("Selected: 3 objects");
    if (ImGui::Button("Select All", ImVec2(-1, 0))) {}
    if (ImGui::Button("Deselect", ImVec2(-1, 0))) {}
    if (ImGui::Button("Invert", ImVec2(-1, 0))) {}
  });

  layout.addToolbarButton("Add", []() {
    ImGui::Text("Primitives:");
    if (ImGui::Button("Cube", ImVec2(-1, 0))) {}
    if (ImGui::Button("Sphere", ImVec2(-1, 0))) {}
    if (ImGui::Button("Plane", ImVec2(-1, 0))) {}
    if (ImGui::Button("Cylinder", ImVec2(-1, 0))) {}
    ImGui::Separator();
    ImGui::Text("Lights:");
    if (ImGui::Button("Point Light", ImVec2(-1, 0))) {}
    if (ImGui::Button("Spot Light", ImVec2(-1, 0))) {}
  });

  layout.addToolbarButton("Object", []() {
    ImGui::InputText("Name", demoText, sizeof(demoText));
    ImGui::Separator();
    ImGui::Text("Transform:");
    ImGui::DragFloat3("Position", demoVec3, 0.1f);
    ImGui::DragFloat3("Rotation", demoVec3, 1.0f);
    ImGui::DragFloat3("Scale", demoVec3, 0.1f);
  });

  layout.addToolbarButton("Physics", []() {
    ImGui::Checkbox("Enabled", &demoBool);
    ImGui::Separator();
    ImGui::SliderFloat("Mass", &demoFloat, 0.0f, 100.0f);
    ImGui::SliderFloat("Friction", &demoFloat, 0.0f, 1.0f);
    ImGui::SliderFloat("Bounce", &demoFloat, 0.0f, 1.0f);
    ImGui::Separator();
    const char* shapes[] = {"Box", "Sphere", "Capsule", "Mesh"};
    ImGui::Combo("Collider", &demoCombo, shapes, IM_ARRAYSIZE(shapes));
  });

  layout.addToolbarButton("Render", []() {
    ImGui::ColorEdit3("Ambient", demoColor);
    ImGui::Separator();
    ImGui::SliderFloat("Exposure", &demoFloat, 0.0f, 5.0f);
    ImGui::SliderFloat("Gamma", &demoFloat, 1.0f, 3.0f);
    ImGui::Separator();
    ImGui::Checkbox("Shadows", &demoBool);
    ImGui::Checkbox("SSAO", &demoBool);
    ImGui::Checkbox("Bloom", &demoBool);
  });

  layout.addToolbarButton("Window", []() {
    ImGui::SliderInt("Width", &demoInt, 640, 1920);
    ImGui::SliderInt("Height", &demoInt, 480, 1080);
    ImGui::Separator();
    ImGui::Checkbox("Fullscreen", &demoBool);
    ImGui::Checkbox("VSync", &demoBool);
    ImGui::SliderInt("FPS Limit", &demoInt, 30, 144);
  });

  layout.addToolbarButton("Help", []() {
    ImGui::Text("Shortcuts:");
    ImGui::BulletText("Ctrl+S - Save");
    ImGui::BulletText("Ctrl+Z - Undo");
    ImGui::BulletText("Ctrl+Y - Redo");
    ImGui::BulletText("Delete - Remove");
    ImGui::Separator();
    ImGui::Text("Version: 1.0.0");
    if (ImGui::Button("Documentation", ImVec2(-1, 0))) {}
  });

  layout.addToolbarButton("Tools", []() {
    if (ImGui::Button("Move (W)", ImVec2(-1, 0))) {}
    if (ImGui::Button("Rotate (E)", ImVec2(-1, 0))) {}
    if (ImGui::Button("Scale (R)", ImVec2(-1, 0))) {}
    ImGui::Separator();
    ImGui::Checkbox("Snap to Grid", &demoBool);
    ImGui::SliderFloat("Grid Size", &demoFloat, 0.1f, 10.0f);
  });

  layout.addToolbarButton("Assets", []() {
    ImGui::Text("Project Assets:");
    if (ImGui::TreeNode("Textures")) {
      ImGui::BulletText("diffuse.png");
      ImGui::BulletText("normal.png");
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Models")) {
      ImGui::BulletText("player.obj");
      ImGui::BulletText("enemy.obj");
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Sounds")) {
      ImGui::BulletText("music.ogg");
      ImGui::BulletText("jump.wav");
      ImGui::TreePop();
    }
  });

  layout.addToolbarButton("Scene", []() {
    ImGui::Text("Hierarchy:");
    if (ImGui::TreeNode("Root")) {
      if (ImGui::TreeNode("Player")) {
        ImGui::BulletText("Camera");
        ImGui::BulletText("Weapon");
        ImGui::TreePop();
      }
      if (ImGui::TreeNode("Environment")) {
        ImGui::BulletText("Ground");
        ImGui::BulletText("Trees");
        ImGui::TreePop();
      }
      ImGui::TreePop();
    }
  });

  layout.addToolbarButton("World", []() {
    ImGui::Text("Environment:");
    ImGui::ColorEdit3("Sky Color", demoColor);
    ImGui::ColorEdit3("Fog Color", demoColor);
    ImGui::SliderFloat("Fog Density", &demoFloat, 0.0f, 1.0f);
    ImGui::Separator();
    ImGui::SliderFloat("Time of Day", &demoFloat, 0.0f, 24.0f);
  });

  layout.addToolbarButton("Debug", []() {
    ImGui::Text("FPS: %d", GetFPS());
    ImGui::Text("Frame Time: %.2f ms", GetFrameTime() * 1000.0f);
    ImGui::Separator();
    ImGui::Checkbox("Show Colliders", &demoBool);
    ImGui::Checkbox("Show FPS", &demoBool);
    ImGui::Checkbox("Wireframe", &demoBool);
    ImGui::Separator();
    if (ImGui::Button("Clear Console", ImVec2(-1, 0))) {}
  });

  layout.addToolbarButton("Extra", []() {
    ImGui::Text("Misc Settings:");
    ImGui::SliderFloat("Volume", &demoFloat, 0.0f, 1.0f);
    ImGui::SliderFloat("Sensitivity", &demoFloat, 0.1f, 5.0f);
    ImGui::Separator();
    const char* langs[] = {"English", "Spanish", "French", "German"};
    ImGui::Combo("Language", &demoCombo, langs, IM_ARRAYSIZE(langs));
  });

  // Add some console output
  layout.log("Editor initialized");
  layout.log("Ready");

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(DARKGRAY);

    rlImGuiBegin();
    layout.render();
    rlImGuiEnd();

    EndDrawing();
  }

  rlImGuiShutdown();
  CloseWindow();
  return 0;
}
