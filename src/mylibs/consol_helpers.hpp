#if 0
cd "$(dirname "$0")/../.." && cmake --build build --target mylibs_tests && ./build/mylibs_tests -tc="console visual test"
exit
#endif
#pragma once
#include <algorithm>
#include <fstream>
#include <functional>
#include <imgui.h>
#include <sstream>
#include <string>
#include <string_view>
#include <typeinfo>
#include <unordered_map>
#include <vector>

struct Console {
  using Args = std::vector<std::string>;
  using CmdFn = std::function<std::string(Args& args)>;

  struct Entry {
    CmdFn fn;
    std::string help;
  };

  static Console& get() {
    static Console instance;
    return instance;
  }

  // ---- Commands ----

  std::unordered_map<std::string, Entry> commands;

  void add(std::string name, CmdFn fn, std::string help = "") { commands[name] = {fn, help}; }

  bool exists(const std::string& name) { return commands.find(name) != commands.end(); }

  std::string exec(const std::string& input) {
    auto tokens = tokenize(input);
    if (tokens.empty())
      return "";
    std::string name = tokens[0];
    Args args(tokens.begin() + 1, tokens.end());

    auto it = commands.find(name);
    if (it == commands.end())
      return "Unknown: " + name;
    return it->second.fn(args);
  }

  static Args tokenize(const std::string& input) {
    Args tokens;
    std::string token;
    bool in_quotes = false;
    char quote_char = 0;

    for (size_t i = 0; i < input.size(); i++) {
      char c = input[i];

      if (!in_quotes && (c == '"' || c == '\'')) {
        in_quotes = true;
        quote_char = c;
      } else if (in_quotes && c == quote_char) {
        in_quotes = false;
        quote_char = 0;
      } else if (!in_quotes && (c == ' ' || c == '\t')) {
        if (!token.empty()) {
          tokens.push_back(token);
          token.clear();
        }
      } else {
        token += c;
      }
    }
    if (!token.empty())
      tokens.push_back(token);
    return tokens;
  }

  // ---- Type-erased context binding ----

  std::unordered_map<std::size_t, void*> contexts;

  template <typename T> void bind(T* obj) {
    contexts[typeid(T).hash_code()] = static_cast<void*>(obj);
  }

  template <typename T> void unbind() { contexts.erase(typeid(T).hash_code()); }

  template <typename T> T* ctx() {
    auto it = contexts.find(typeid(T).hash_code());
    if (it == contexts.end())
      return nullptr;
    return static_cast<T*>(it->second);
  }

  // ---- Source registration ----

  template <typename T> void register_source(T& source) {
    bind(&source);
    source.register_commands(*this);
  }

  // ---- Terminal state ----

  std::vector<std::string> log;
  std::vector<std::string> history;
  int historyIndex = -1;
  char inputBuf[256] = {0};
  bool scrollToBottom = false;
  bool focusInput = true;
  bool visible = false;

  void toggle_visible() {
    visible = !visible;
    if (visible)
      focusInput = true;
  }

  void print(const std::string& msg) {
    log.push_back(msg);
    scrollToBottom = true;
  }

  void clear() { log.clear(); }

  void execute(const std::string& input) {
    print("> " + input);
    history.push_back(input);
    historyIndex = -1;
    std::string result = exec(input);
    if (!result.empty())
      print(result);
  }

  // ---- ImGui Drawing ----

  void draw_imgui() {
    if (!visible)
      return;

    if (ImGui::Begin("Console", &visible)) {
      float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
      ImGui::BeginChild("log_region", ImVec2(0, -footerHeight), true);

      for (auto& line : log) {
        if (!line.empty() && line[0] == '>') {
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
          ImGui::TextWrapped("%s", line.c_str());
          ImGui::PopStyleColor();
        } else if (line.find("Unknown") != std::string::npos ||
                   line.find("Error") != std::string::npos ||
                   line.find("Usage") != std::string::npos ||
                   line.find("not bound") != std::string::npos) {
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
          ImGui::TextWrapped("%s", line.c_str());
          ImGui::PopStyleColor();
        } else {
          ImGui::TextWrapped("%s", line.c_str());
        }
      }

      if (scrollToBottom) {
        ImGui::SetScrollHereY(1.0f);
        scrollToBottom = false;
      }
      ImGui::EndChild();

      ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                  ImGuiInputTextFlags_CallbackHistory |
                                  ImGuiInputTextFlags_CallbackCompletion;

      if (focusInput) {
        ImGui::SetKeyboardFocusHere();
        focusInput = false;
      }

      ImGui::PushItemWidth(-1);
      if (ImGui::InputText(
              "##cmdinput", inputBuf, sizeof(inputBuf), flags,
              [](ImGuiInputTextCallbackData* data) -> int {
                Console* c = (Console*)data->UserData;

                if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
                  if (c->history.empty())
                    return 0;
                  if (data->EventKey == ImGuiKey_UpArrow) {
                    if (c->historyIndex < 0)
                      c->historyIndex = (int)c->history.size() - 1;
                    else if (c->historyIndex > 0)
                      c->historyIndex--;
                  } else if (data->EventKey == ImGuiKey_DownArrow) {
                    if (c->historyIndex >= 0)
                      c->historyIndex++;
                    if (c->historyIndex >= (int)c->history.size())
                      c->historyIndex = -1;
                  }
                  const char* str =
                      (c->historyIndex >= 0) ? c->history[c->historyIndex].c_str() : "";
                  data->DeleteChars(0, data->BufTextLen);
                  data->InsertChars(0, str);
                }

                if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
                  std::string partial(data->Buf, data->CursorPos);
                  std::vector<std::string> matches;
                  for (auto& [name, entry] : c->commands) {
                    if (name.substr(0, partial.size()) == partial) {
                      matches.push_back(name);
                    }
                  }
                  if (matches.size() == 1) {
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, matches[0].c_str());
                    data->InsertChars(data->CursorPos, " ");
                  } else if (matches.size() > 1) {
                    std::sort(matches.begin(), matches.end());
                    std::string list;
                    for (auto& m : matches)
                      list += m + "  ";
                    c->print(list);
                  }
                }

                return 0;
              },
              this)) {
        if (inputBuf[0]) {
          execute(inputBuf);
          inputBuf[0] = 0;
        }
        focusInput = true;
      }
      ImGui::PopItemWidth();
    }
    ImGui::End();
  }

private:
  Console() = default;
};

// ---- Auto-registration ----

struct AutoCmd {
  AutoCmd(std::string name, Console::CmdFn fn, std::string help = "") {
    Console::get().add(name, fn, help);
  }
};

#define REGISTER_CMD(name, help, ...)                                                              \
  static AutoCmd _cmd_##name(#name, [](Console::Args & args) -> std::string __VA_ARGS__, help)

#define CMD_CTX(Type, var)                                                                         \
  auto* var##_ptr = Console::get().ctx<Type>();                                                    \
  if (!var##_ptr)                                                                                  \
    return std::string(#Type " not bound");                                                        \
  auto& var = *var##_ptr

// ---- Built-in commands ----

REGISTER_CMD(help, "list all commands", {
  (void)args;
  std::string out;
  std::vector<std::pair<std::string, std::string>> sorted;
  for (auto& [name, entry] : Console::get().commands) {
    sorted.push_back({name, entry.help});
  }
  std::sort(sorted.begin(), sorted.end());
  for (auto& [name, h] : sorted) {
    out += name + " - " + h + "\n";
  }
  if (!out.empty() && out.back() == '\n')
    out.pop_back();
  return out;
});

REGISTER_CMD(echo, "echo args", {
  std::string out;
  for (size_t i = 0; i < args.size(); i++) {
    if (i > 0)
      out += " ";
    out += args[i];
  }
  return out;
});

REGISTER_CMD(clear_console, "clear console log", {
  (void)args;
  Console::get().clear();
  return "";
});

REGISTER_CMD(run, "run <file> - execute commands from text file", {
  if (args.empty())
    return std::string("Usage: run <file.txt>");

  std::ifstream file(args[0]);
  if (!file.is_open())
    return "Failed to open: " + args[0];

  std::string line;
  int executed = 0;
  int errors = 0;

  while (std::getline(file, line)) {
    // Skip empty lines and comments
    if (line.empty())
      continue;
    // Trim leading whitespace
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos)
      continue;
    line = line.substr(start);
    // Skip comments
    if (line[0] == '#' || (line.size() > 1 && line[0] == '/' && line[1] == '/'))
      continue;

    Console::get().execute(line);
    executed++;
  }

  return "Executed " + std::to_string(executed) + " commands from " + args[0];
});

// ============================================================================
// DOCTEST
// ============================================================================
#ifdef DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

struct DemoPlayer {
  int health = 100;
  float x = 0, y = 0, z = 0;
};

REGISTER_CMD(hp, "get/set health", {
  CMD_CTX(DemoPlayer, p);
  if (args.empty())
    return "Health: " + std::to_string(p.health);
  p.health = std::stoi(args[0]);
  return "Health set to " + args[0];
});

REGISTER_CMD(tp, "teleport <x> <y> <z>", {
  CMD_CTX(DemoPlayer, p);
  if (args.size() < 3)
    return std::string("Usage: tp <x> <y> <z>");
  p.x = std::stof(args[0]);
  p.y = std::stof(args[1]);
  p.z = std::stof(args[2]);
  return "Teleported to " + args[0] + " " + args[1] + " " + args[2];
});

REGISTER_CMD(pos, "show position", {
  CMD_CTX(DemoPlayer, p);
  (void)args;
  return std::to_string(p.x) + " " + std::to_string(p.y) + " " + std::to_string(p.z);
});

TEST_CASE("console visual test") {
  const int screenWidth = 1024;
  const int screenHeight = 768;

  InitWindow(screenWidth, screenHeight, "Console Test");
  SetTargetFPS(60);
  rlImGuiSetup(true);

  DemoPlayer player;
  Console::get().bind<DemoPlayer>(&player);

  Console::get().visible = true;
  Console::get().print("Type 'help' for commands. Press ~ to toggle.");

  Color bgColor = DARKGRAY;

  Console::get().add(
      "color",
      [&](Console::Args& a) -> std::string {
        if (a.size() < 3)
          return "Usage: color <r> <g> <b>";
        bgColor = {(unsigned char)std::stoi(a[0]), (unsigned char)std::stoi(a[1]),
                   (unsigned char)std::stoi(a[2]), 255};
        return "Color set";
      },
      "color <r> <g> <b>");

  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_GRAVE))
      Console::get().toggle_visible();

    BeginDrawing();
    ClearBackground(bgColor);

    DrawText(TextFormat("Player HP: %d", player.health), 10, 10, 20, WHITE);
    DrawText(TextFormat("Pos: %.1f %.1f %.1f", player.x, player.y, player.z), 10, 40, 20, WHITE);

    rlImGuiBegin();
    Console::get().draw_imgui();
    rlImGuiEnd();

    DrawFPS(screenWidth - 100, 10);
    EndDrawing();
  }

  Console::get().unbind<DemoPlayer>();
  rlImGuiShutdown();
  CloseWindow();
  CHECK(true);
}

#endif
