#pragma once
/**
 * @brief Lua VM core: owns a single lua_State and exposes lifecycle, script running,
 * REPL-style eval with print capture, and a registry for C bindings.
 *
 * This header is game-agnostic. It knows nothing about entities, models, traits or
 * raylib. A game registers its own bindings with register_fn() (before or after init())
 * and routes output with set_log(). See src/scratch/entity_demo/lua_bindings.inc for
 * an example binding table.
 */

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace LuaAPI {

inline lua_State* L = nullptr;

// ---- logging ----
// The only output channel the core knows about. Errors, print() and anything a binding
// wants to report go through log(); the game decides where that lands (console, stderr).

using LogFn = void (*)(const std::string&);

inline void default_log(const std::string& msg) { std::fprintf(stderr, "%s\n", msg.c_str()); }
inline LogFn log_fn = default_log;
inline void set_log(LogFn fn) { log_fn = fn ? fn : default_log; }
inline void log(const std::string& msg) { log_fn(msg); }

// ---- binding registry ----
// register_fn() works before or after init(): entries are kept in a vector that init()
// applies, and if the VM already exists the function is registered immediately.
// Registering a name twice replaces the earlier entry (so a game may override help/print).

struct Binding {
  const char* name;
  lua_CFunction fn;
  const char* sig; // argument list as the user types it, shown by help()
  const char* doc; // one line, shown by help()
};

inline std::vector<Binding>& bindings() {
  static std::vector<Binding> v;
  return v;
}

inline void register_fn(const char* name, lua_CFunction fn, const char* sig = "",
                        const char* doc = "") {
  Binding b{name, fn, sig, doc};
  auto& v = bindings();
  auto it = std::find_if(v.begin(), v.end(),
                         [&](const Binding& e) { return strcmp(e.name, name) == 0; });
  if (it != v.end())
    *it = b;
  else
    v.push_back(b);
  if (L)
    lua_register(L, name, fn);
}

inline const Binding* find_binding(const char* name) {
  for (const auto& b : bindings())
    if (strcmp(b.name, name) == 0)
      return &b;
  return nullptr;
}

// Text around the help() listing. A game may replace these with its own hints.
inline const char* help_header = "Lua API";
inline const char* help_footer = "Try: help(\"name\") for one function's signature and doc";

// ---- history ----
// Every chunk that ran through eval() or run_file() is recorded with what it produced,
// so a UI (see lua_imgui.hpp) can show what Lua has executed, from any source.

struct HistoryEntry {
  std::string code;   // the chunk as submitted (or "dofile <path>")
  std::string output; // captured print()/return values or the error text
  bool ok = true;
};

inline size_t history_limit = 500; // oldest entries are dropped past this

inline std::vector<HistoryEntry>& history() {
  static std::vector<HistoryEntry> h;
  return h;
}

inline void record(std::string code, std::string output, bool ok) {
  auto& h = history();
  h.push_back(HistoryEntry{std::move(code), std::move(output), ok});
  if (h.size() > history_limit)
    h.erase(h.begin(), h.begin() + (long)(h.size() - history_limit));
}

// ---- print capture ----
// Lua's global print is replaced so output goes through log() and, while an eval() is
// in flight, into a capture buffer that is returned to the caller.

inline std::string* capture_buf = nullptr;

static int l_print(lua_State* L) {
  int n = lua_gettop(L);
  std::string line;
  for (int i = 1; i <= n; i++) {
    size_t len = 0;
    const char* str = luaL_tolstring(L, i, &len); // pushes the string
    if (i > 1)
      line += "\t";
    line.append(str ? str : "nil", str ? len : 3);
    lua_pop(L, 1);
  }
  log(line);
  if (capture_buf) {
    if (!capture_buf->empty())
      *capture_buf += "\n";
    *capture_buf += line;
  }
  return 0;
}

// help([name]) -> string
static int l_help(lua_State* L) {
  const char* want = luaL_optstring(L, 1, nullptr);
  std::string out;
  if (want) {
    if (const Binding* b = find_binding(want)) {
      out = std::string(b->sig) + "\n  " + b->doc;
      lua_pushstring(L, out.c_str());
      return 1;
    }
    lua_pushstring(L, ("no such function: " + std::string(want)).c_str());
    return 1;
  }
  out += help_header;
  out += "\n";
  size_t w = 0;
  for (const auto& b : bindings())
    w = std::max(w, strlen(b.sig));
  for (const auto& b : bindings()) {
    out += "  " + std::string(b.sig);
    out.append(w - strlen(b.sig) + 2, ' ');
    out += b.doc;
    out += "\n";
  }
  out += help_footer;
  lua_pushstring(L, out.c_str());
  return 1;
}

// ---- lifecycle ----

inline void init() {
  L = luaL_newstate();
  luaL_openlibs(L);

  // core bindings first, so anything the game registered under the same name wins
  // via replace-by-name when the vector is applied below
  Binding core[] = {
      {"help", l_help, "help([name])", "this list, or details for one function"},
      {"print", l_print, "print(...)", "print to the log and to the caller of eval()"},
  };
  for (const auto& c : core)
    if (!find_binding(c.name))
      bindings().insert(bindings().begin(), c);

  for (const auto& b : bindings())
    lua_register(L, b.name, b.fn);
}

inline void shutdown() {
  if (L) {
    lua_close(L);
    L = nullptr;
  }
  // bindings() is kept so shutdown()+init() re-registers everything
}

inline bool run_file(const std::string& path) {
  if (!L) {
    log("lua: not initialized");
    return false;
  }
  int result = luaL_dofile(L, path.c_str());
  if (result != LUA_OK) {
    const char* err = lua_tostring(L, -1);
    std::string msg = "lua error: " + std::string(err ? err : "unknown");
    log(msg);
    lua_pop(L, 1);
    record("dofile " + path, msg, false);
    return false;
  }
  record("dofile " + path, "", true);
  return true;
}

inline bool run_string(const std::string& code) {
  if (!L) {
    log("lua: not initialized");
    return false;
  }
  int result = luaL_dostring(L, code.c_str());
  if (result != LUA_OK) {
    const char* err = lua_tostring(L, -1);
    log("lua error: " + std::string(err ? err : "unknown"));
    lua_pop(L, 1);
    return false;
  }
  return true;
}

// Evaluate a chunk REPL-style and return everything it produced as text:
// captured print() output, then the chunk's return values (one per line), or
// "lua error: ..." on failure. Like lua.c, "return <code>" is tried first so a
// bare expression such as `entity_count()` or `1+1` shows its value.
inline std::string eval(const std::string& code) {
  if (!L)
    return "lua: not initialized";

  std::string out;
  std::string* prev_capture = capture_buf;
  capture_buf = &out;

  int base = lua_gettop(L);
  int status = luaL_loadstring(L, ("return " + code).c_str());
  if (status != LUA_OK) {
    lua_pop(L, 1); // discard syntax error from the "return" attempt
    status = luaL_loadstring(L, code.c_str());
  }
  if (status == LUA_OK)
    status = lua_pcall(L, 0, LUA_MULTRET, 0);

  if (status != LUA_OK) {
    const char* err = lua_tostring(L, -1);
    std::string msg = "lua error: " + std::string(err ? err : "unknown");
    log(msg);
    if (!out.empty())
      out += "\n";
    out += msg;
    lua_pop(L, 1);
  } else {
    int nresults = lua_gettop(L) - base;
    for (int i = 1; i <= nresults; i++) {
      size_t len = 0;
      const char* str = luaL_tolstring(L, base + i, &len);
      std::string val(str ? str : "nil", str ? len : 3);
      lua_pop(L, 1);
      log(val);
      if (!out.empty())
        out += "\n";
      out += val;
    }
  }
  lua_settop(L, base);
  capture_buf = prev_capture;
  record(code, out, status == LUA_OK);
  return out;
}

} // namespace LuaAPI
