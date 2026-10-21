#pragma once
#include <raylib.h>
#include <rlgl.h>
#include <cstring>
#include <string>

/**
 * @brief One GLSL source for desktop and web.
 *
 * The shader files in assets/shaders are written as GLSL 330. GLSL ES 3.00 (WebGL 2) is the
 * same language with a different header: `#version 300 es` plus default precisions. So the
 * web build rewrites the header at load time instead of keeping a second copy of every shader.
 * On desktop the source is used as-is.
 */
namespace ShaderPortable {

inline constexpr const char* DESKTOP_HEADER = "#version 330";
inline constexpr const char* ES3_HEADER =
    "#version 300 es\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "precision highp sampler2D;";

/// Swap the `#version 330` header for the ES 3.00 one when building for the web.
inline std::string adapt(const char* src) {
  if (!src)
    return {};
#if defined(__EMSCRIPTEN__)
  std::string s(src);
  size_t n = std::strlen(DESKTOP_HEADER);
  if (s.compare(0, n, DESKTOP_HEADER) == 0)
    return ES3_HEADER + s.substr(n);
  return s;
#else
  return std::string(src);
#endif
}

/// LoadShaderFromMemory with the header adapted; either source may be null (raylib default).
inline Shader load_memory(const char* vs_src, const char* fs_src) {
  std::string vs = adapt(vs_src), fs = adapt(fs_src);
  return LoadShaderFromMemory(vs_src ? vs.c_str() : nullptr, fs_src ? fs.c_str() : nullptr);
}

/// LoadShader from files with the header adapted; either path may be null (raylib default).
inline Shader load(const char* vs_path, const char* fs_path) {
  char* vs = vs_path ? LoadFileText(vs_path) : nullptr;
  char* fs = fs_path ? LoadFileText(fs_path) : nullptr;
  if ((vs_path && !vs) || (fs_path && !fs)) {
    TraceLog(LOG_WARNING, "SHADER: could not read %s / %s", vs_path ? vs_path : "(default)",
             fs_path ? fs_path : "(default)");
  }
  Shader sh = load_memory(vs, fs);
  if (vs)
    UnloadFileText(vs);
  if (fs)
    UnloadFileText(fs);
  return sh;
}

} // namespace ShaderPortable
