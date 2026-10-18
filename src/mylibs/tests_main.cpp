#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

// Raylib/ImGui for visual tests
#include "raylib.h"
#include "imgui.h"
#include "rlImGui.h"

// Include headers with embedded tests
#include "ilist.hpp"
#include "hexgrid_math.hpp"
#include "asset_helpers.hpp"
#include "zoo.hpp"
#include "game_console_api.hpp"
#include "model_api.hpp"
#include "lua_api.hpp"

// ---- Lua core (no game bindings, no raylib) ----

static std::string lua_test_log;
static void lua_test_logger(const std::string& m) {
  lua_test_log += m;
  lua_test_log += "\n";
}
static int l_twice(lua_State* L) {
  lua_pushinteger(L, 2 * luaL_checkinteger(L, 1));
  return 1;
}
static int l_hello(lua_State* L) {
  lua_pushstring(L, "hello");
  return 1;
}

TEST_CASE("lua core - register eval print help") {
  lua_test_log.clear();
  LuaAPI::set_log(lua_test_logger);

  // registering before init() is applied by init()
  LuaAPI::register_fn("twice", l_twice, "twice(n) -> 2n", "double a number");
  LuaAPI::init();
  REQUIRE(LuaAPI::L != nullptr);
  CHECK(LuaAPI::eval("twice(2)") == "4");

  // registering after init() is live immediately
  LuaAPI::register_fn("hello", l_hello, "hello() -> string", "say hello");
  CHECK(LuaAPI::eval("hello()") == "hello");

  // re-registering a name replaces, not duplicates
  size_t before = LuaAPI::bindings().size();
  LuaAPI::register_fn("hello", l_hello, "hello() -> string", "say hello again");
  CHECK(LuaAPI::bindings().size() == before);
  CHECK(std::string(LuaAPI::find_binding("hello")->doc) == "say hello again");

  // bare expressions and statements
  CHECK(LuaAPI::eval("1+1") == "2");
  CHECK(LuaAPI::eval("x = 5") == "");
  CHECK(LuaAPI::eval("x") == "5");

  // print is captured and also logged
  lua_test_log.clear();
  CHECK(LuaAPI::eval("print('a', 1)") == "a\t1");
  CHECK(lua_test_log == "a\t1\n");

  // help
  std::string h = LuaAPI::eval("help('twice')");
  CHECK(h.find("twice(n) -> 2n") != std::string::npos);
  CHECK(h.find("double a number") != std::string::npos);
  std::string all = LuaAPI::eval("help()");
  CHECK(all.find(LuaAPI::help_header) == 0);
  CHECK(all.find("hello() -> string") != std::string::npos);
  CHECK(all.find("help([name])") != std::string::npos);
  CHECK(LuaAPI::eval("help('nope')") == "no such function: nope");

  // errors are returned, logged, and leave the stack clean
  lua_test_log.clear();
  std::string err = LuaAPI::eval("nosuch()");
  CHECK(err.rfind("lua error:", 0) == 0);
  CHECK(lua_test_log.rfind("lua error:", 0) == 0);
  CHECK(lua_gettop(LuaAPI::L) == 0);
  CHECK(LuaAPI::run_string("this is not lua") == false);
  CHECK(LuaAPI::run_string("y = twice(21)") == true);
  CHECK(LuaAPI::eval("y") == "42");

  // shutdown + init re-registers everything kept in the vector
  LuaAPI::shutdown();
  CHECK(LuaAPI::L == nullptr);
  CHECK(LuaAPI::eval("1") == "lua: not initialized");
  LuaAPI::init();
  CHECK(LuaAPI::eval("twice(4)") == "8");
  LuaAPI::shutdown();
  LuaAPI::set_log(nullptr);
}

TEST_CASE("lua core - in-game console fallback and history") {
  LuaAPI::set_log(lua_test_logger);
  LuaAPI::history().clear();
  LuaAPI::register_fn("twice", l_twice, "twice(n) -> 2n", "double a number");
  LuaAPI::init();
  GameConsoleAPI::fallback() = [](const std::string& code) { return LuaAPI::eval(code); };

  // what the in-game console does with typed input that is not a registered command
  CHECK(GameConsoleAPI::exec("twice(21)") == "42");
  CHECK(GameConsoleAPI::exec("lua 1+1") == "2");        // explicit prefix
  CHECK(GameConsoleAPI::exec("echo still a command") == "still a command");
  CHECK(GameConsoleAPI::exec("bogus(").rfind("lua error:", 0) == 0);

  // console execute() (what pressing Enter does) also lands in Lua history
  GameConsoleAPI::execute("twice(5)");

  // the echo command never reached Lua, so it is not in the history
  auto& h = LuaAPI::history();
  REQUIRE(h.size() == 4);
  CHECK(h[0].code == "twice(21)");
  CHECK(h[0].output == "42");
  CHECK(h[0].ok);
  CHECK(h[1].code == "1+1");
  CHECK_FALSE(h[2].ok);
  CHECK(h[2].output.rfind("lua error:", 0) == 0);
  CHECK(h[3].code == "twice(5)");
  CHECK(h[3].output == "10");

  // run_file is recorded too, including a missing file
  CHECK_FALSE(LuaAPI::run_file("/nonexistent/x.lua"));
  CHECK(h.back().code == "dofile /nonexistent/x.lua");
  CHECK_FALSE(h.back().ok);

  // bounded
  LuaAPI::history_limit = 3;
  for (int i = 0; i < 10; i++)
    LuaAPI::eval("1");
  CHECK(h.size() == 3);
  LuaAPI::history_limit = 500;

  GameConsoleAPI::fallback() = nullptr;
  LuaAPI::shutdown();
  LuaAPI::set_log(nullptr);
}
