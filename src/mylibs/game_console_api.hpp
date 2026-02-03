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

/**
 * @brief In-game developer console with command registration, type-erased context binding, and
 * ImGui terminal.
 *
 * Namespace-style API with all static functions.
 * Commands are registered via add() or the REGISTER_CMD macro (which uses AutoCmd).
 * Runtime context is bound via bind<T>() and retrieved via ctx<T>().
 *
 * @see AutoCmd, REGISTER_CMD, CMD_CTX
 */
struct GameConsoleAPI {
  using Args = std::vector<std::string>;
  using CmdFn = std::function<std::string(Args& args)>;

  struct Entry {
    CmdFn fn;
    std::string help;
  };

private:
  static std::unordered_map<std::size_t, void*>& contexts() {
    static std::unordered_map<std::size_t, void*> c;
    return c;
  }

  static std::vector<std::string>& log() {
    static std::vector<std::string> l;
    return l;
  }

  static std::vector<std::string>& history() {
    static std::vector<std::string> h;
    return h;
  }

  static int& historyIndex() {
    static int i = -1;
    return i;
  }

  static char* inputBuf() {
    static char buf[256] = {0};
    return buf;
  }

  static bool& scrollToBottom() {
    static bool s = false;
    return s;
  }

  static bool& focusInput() {
    static bool f = true;
    return f;
  }

  static bool& visibleFlag() {
    static bool v = false;
    return v;
  }

public:
  // ---- Commands ----

  static std::unordered_map<std::string, Entry>& commands() {
    static std::unordered_map<std::string, Entry> c;
    return c;
  }

  static void add(std::string name, CmdFn fn, std::string help = "") {
    commands()[name] = {fn, help};
  }

  static bool exists(const std::string& name) {
    return commands().find(name) != commands().end();
  }

  static std::string exec(const std::string& input) {
    auto tokens = tokenize(input);
    if (tokens.empty())
      return "";
    std::string name = tokens[0];
    Args args(tokens.begin() + 1, tokens.end());
    auto it = commands().find(name);
    if (it == commands().end())
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

  template <typename T> static void bind(T* obj) {
    contexts()[typeid(T).hash_code()] = static_cast<void*>(obj);
  }

  template <typename T> static void unbind() { contexts().erase(typeid(T).hash_code()); }

  template <typename T> static T* ctx() {
    auto it = contexts().find(typeid(T).hash_code());
    if (it == contexts().end())
      return nullptr;
    return static_cast<T*>(it->second);
  }

  // ---- Source registration ----

  template <typename T> static void register_source(T& source) {
    bind(&source);
    source.register_commands();
  }

  // ---- Terminal state ----

  static bool visible() { return visibleFlag(); }

  static void toggle_visible() {
    visibleFlag() = !visibleFlag();
    if (visibleFlag())
      focusInput() = true;
  }

  static void print(const std::string& msg) {
    log().push_back(msg);
    scrollToBottom() = true;
  }

  static void clear() { log().clear(); }

  static void execute(const std::string& input) {
    print("> " + input);
    history().push_back(input);
    historyIndex() = -1;
    std::string result = exec(input);
    if (!result.empty())
      print(result);
  }

  // ---- ImGui Drawing ----

  static void draw_imgui() {
    if (!visibleFlag())
      return;

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Console", &visibleFlag())) {
      // Log area
      float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
      ImGui::BeginChild("LogRegion", ImVec2(0, -footerHeight), false,
                        ImGuiWindowFlags_HorizontalScrollbar);
      for (const auto& line : log()) {
        ImGui::TextUnformatted(line.c_str());
      }
      if (scrollToBottom()) {
        ImGui::SetScrollHereY(1.0f);
        scrollToBottom() = false;
      }
      ImGui::EndChild();

      // Input
      ImGui::Separator();
      bool reclaimFocus = false;
      ImGuiInputTextFlags flags =
          ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;

      if (focusInput()) {
        ImGui::SetKeyboardFocusHere();
        focusInput() = false;
      }

      if (ImGui::InputText("##Input", inputBuf(), 256, flags,
                           [](ImGuiInputTextCallbackData* data) -> int {
                             if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
                               auto& hist = history();
                               auto& idx = historyIndex();
                               if (data->EventKey == ImGuiKey_UpArrow) {
                                 if (idx < 0)
                                   idx = (int)hist.size() - 1;
                                 else if (idx > 0)
                                   idx--;
                               } else if (data->EventKey == ImGuiKey_DownArrow) {
                                 if (idx >= 0)
                                   idx++;
                                 if (idx >= (int)hist.size())
                                   idx = -1;
                               }
                               if (idx >= 0 && idx < (int)hist.size()) {
                                 data->DeleteChars(0, data->BufTextLen);
                                 data->InsertChars(0, hist[idx].c_str());
                               } else if (idx < 0) {
                                 data->DeleteChars(0, data->BufTextLen);
                               }
                             }
                             return 0;
                           })) {
        std::string cmd = inputBuf();
        if (!cmd.empty()) {
          execute(cmd);
          inputBuf()[0] = '\0';
        }
        reclaimFocus = true;
      }

      ImGui::SetItemDefaultFocus();
      if (reclaimFocus)
        ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::End();
  }
};

// ---- Auto-registration ----

struct AutoCmd {
  AutoCmd(std::string name, GameConsoleAPI::CmdFn fn, std::string help = "") {
    GameConsoleAPI::add(name, fn, help);
  }
};

#define REGISTER_CMD(name, help, ...)                                                              \
  static AutoCmd _cmd_##name(#name, [](GameConsoleAPI::Args & args) -> std::string __VA_ARGS__, help)

#define CMD_CTX(Type, var)                                                                         \
  auto* var##_ptr = GameConsoleAPI::ctx<Type>();                                                   \
  if (!var##_ptr)                                                                                  \
    return std::string(#Type " not bound");                                                        \
  auto& var = *var##_ptr

// ---- Built-in commands ----

REGISTER_CMD(help, "list all commands", {
  (void)args;
  std::string out;
  std::vector<std::pair<std::string, std::string>> sorted;
  for (auto& [name, entry] : GameConsoleAPI::commands()) {
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
  GameConsoleAPI::clear();
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

  while (std::getline(file, line)) {
    if (line.empty())
      continue;
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos)
      continue;
    line = line.substr(start);
    if (line[0] == '#' || (line.size() > 1 && line[0] == '/' && line[1] == '/'))
      continue;

    GameConsoleAPI::execute(line);
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
  GameConsoleAPI::bind<DemoPlayer>(&player);

  GameConsoleAPI::toggle_visible();
  GameConsoleAPI::print("Type 'help' for commands. Press ~ to toggle.");

  Color bgColor = DARKGRAY;

  GameConsoleAPI::add(
      "color",
      [&](GameConsoleAPI::Args& a) -> std::string {
        if (a.size() < 3)
          return "Usage: color <r> <g> <b>";
        bgColor = {(unsigned char)std::stoi(a[0]), (unsigned char)std::stoi(a[1]),
                   (unsigned char)std::stoi(a[2]), 255};
        return "Color set";
      },
      "color <r> <g> <b>");

  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_GRAVE))
      GameConsoleAPI::toggle_visible();

    BeginDrawing();
    ClearBackground(bgColor);

    DrawText(TextFormat("Player HP: %d", player.health), 10, 10, 20, WHITE);
    DrawText(TextFormat("Pos: %.1f %.1f %.1f", player.x, player.y, player.z), 10, 40, 20, WHITE);

    rlImGuiBegin();
    GameConsoleAPI::draw_imgui();
    rlImGuiEnd();

    DrawFPS(screenWidth - 100, 10);
    EndDrawing();
  }

  GameConsoleAPI::unbind<DemoPlayer>();
  rlImGuiShutdown();
  CloseWindow();
  CHECK(true);
}

#endif
