#!/usr/bin/env python3
"""Generate a game's entity trait struct from tags found in its sources.

Usage: gen_traits.py --src <dir> --out <file>

Scans *.cpp *.hpp *.inc under --src (skipping any gen/ directory) for:

    DECLARE_TRAIT(IDENT, "doc")   exactly one per trait; placed next to the code that first needed it
    HAS_TRAIT(expr, IDENT)        usage sites, recorded so the generated header links back to them
    ADD_TRAIT(expr, IDENT)
    RM_TRAIT(expr, IDENT)

and writes a header with `enum class TraitId`, `struct TraitBits` (one bit per trait), a
`TRAIT_INFO` table (name/doc/declared-at) and a `TRAIT_SITES` table (every usage).
The Lua/console name of a trait is its identifier lower-cased.

Errors (file:line: error: ...) exit 1: an invalid identifier, a usage without a DECLARE_TRAIT,
or a duplicate DECLARE_TRAIT. A trait declared but never used is only a warning.
"""

import argparse
import os
import re
import sys

TAG_RE = re.compile(r"\b(DECLARE_TRAIT|HAS_TRAIT|ADD_TRAIT|RM_TRAIT)\s*\(")
IDENT_RE = re.compile(r"^[A-Z][A-Z0-9_]*$")
EXTS = (".cpp", ".hpp", ".inc")


def strip_comments(text):
    """Replace comments with spaces (newlines kept) so line numbers survive."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join("\n" if ch == "\n" else " " for ch in text[i:j]))
            i = j
        elif c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            out.append(text[i : j + 1])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def blank_preprocessor_lines(text):
    """Blank `#define ...` lines (the macro definitions themselves are not tags)."""
    return "\n".join("" if re.match(r"\s*#\s*define\b", ln) else ln for ln in text.split("\n"))


def split_args(s):
    """Split a macro argument list on top-level commas. Returns (args, end_index) or None."""
    depth, cur, args, i, n = 0, [], [], 0, len(s)
    while i < n:
        c = s[i]
        if c == '"':
            j = i + 1
            while j < n and s[j] != '"':
                j += 2 if s[j] == "\\" else 1
            cur.append(s[i : j + 1])
            i = j + 1
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                args.append("".join(cur).strip())
                return args, i
            depth -= 1
        elif c == "," and depth == 0:
            args.append("".join(cur).strip())
            cur = []
            i += 1
            continue
        cur.append(c)
        i += 1
    return None


def scan(src_dir):
    declares = {}  # ident -> dict(file, line, doc)
    sites = []  # (ident, file, line, kind)
    errors = []
    files = []
    for root, dirs, names in os.walk(src_dir):
        dirs[:] = sorted(d for d in dirs if d != "gen" and d != "__pycache__")
        for name in sorted(names):
            if name.endswith(EXTS):
                files.append(os.path.join(root, name))
    for path in files:
        rel = os.path.relpath(path, src_dir)
        with open(path, encoding="utf-8", errors="replace") as f:
            text = blank_preprocessor_lines(strip_comments(f.read()))
        for m in TAG_RE.finditer(text):
            line = text.count("\n", 0, m.start()) + 1
            kind = m.group(1)
            parsed = split_args(text[m.end() :])
            if parsed is None:
                errors.append(f"{rel}:{line}: error: unterminated {kind}(")
                continue
            args, _ = parsed
            if len(args) != 2:
                errors.append(f"{rel}:{line}: error: {kind} takes 2 arguments, got {len(args)}")
                continue
            if kind == "DECLARE_TRAIT":
                ident, doc = args
            else:
                ident = args[1]
            if not IDENT_RE.match(ident):
                errors.append(f"{rel}:{line}: error: {kind}: bad trait identifier '{ident}' (want UPPER_SNAKE)")
                continue
            if kind == "DECLARE_TRAIT":
                if not (doc.startswith('"') and doc.endswith('"')):
                    errors.append(f"{rel}:{line}: error: DECLARE_TRAIT({ident}, ...) doc must be a string literal")
                    continue
                if ident in declares:
                    d = declares[ident]
                    errors.append(f"{rel}:{line}: error: duplicate DECLARE_TRAIT({ident}); first at {d['file']}:{d['line']}")
                    continue
                declares[ident] = {"file": rel, "line": line, "doc": doc}
            else:
                sites.append((ident, rel, line, kind.split("_")[0]))
    for ident, rel, line, kind in sites:
        if ident not in declares:
            errors.append(f"{rel}:{line}: error: {kind}_TRAIT({ident}) has no DECLARE_TRAIT anywhere under {os.path.basename(src_dir)}/")
    used = {s[0] for s in sites}
    for ident, d in declares.items():
        if ident not in used:
            print(f"{d['file']}:{d['line']}: warning: DECLARE_TRAIT({ident}) is never used", file=sys.stderr)
    return declares, sites, errors


def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def emit(declares, sites, src_name):
    idents = sorted(declares)
    by_ident = {i: [s for s in sites if s[0] == i] for i in idents}
    L = []
    w = L.append
    page = f"{src_name}_traits_gen"
    group = f"{src_name}_traits_api"
    cell = lambda t: t.replace("|", "/")
    w(f"// GENERATED by tools/gen_traits.py from {src_name}. Do not edit; edit the DECLARE_TRAIT /")
    w("// HAS_TRAIT / ADD_TRAIT / RM_TRAIT tags in the sources and rebuild.")
    w("#pragma once")
    w("#include <cstdint>")
    w("#include <cstring>")
    w("")
    # ---- Doxygen: a page describing the generated struct, and a group holding its API ----
    w("/**")
    w(f" * @file entity_traits.gen.hpp")
    w(f" * @brief Generated trait set for `{src_name}`: see @ref {page}.")
    w(" */")
    w("")
    w("/**")
    w(f" * @page {page} Generated entity traits: {src_name}")
    w(" *")
    w(f" * This struct is **generated**, not written. `tools/gen_traits.py` scans `src/scratch/{src_name}/`")
    w(" * for `DECLARE_TRAIT(IDENT, \"doc\")` tags and for the `HAS_TRAIT` / `ADD_TRAIT` / `RM_TRAIT` use")
    w(f" * sites, then writes `gen/entity_traits.gen.hpp`. CMake reruns it whenever a source in that folder")
    w(" * changes (see `game_gen_traits()` in CMakeLists.txt), so this page always reflects the tags.")
    w(" *")
    w(" * To add a trait: put `DECLARE_TRAIT(FOO, \"why it exists\")` right above the first `HAS_TRAIT(e, FOO)`")
    w(" * that needs it and rebuild. Nothing else has to be edited. The declaring line is the trait's origin")
    w(" * and is recorded below so every trait links back to the code that demanded it.")
    w(" *")
    w(f" * @section {page}_table Traits ({len(idents)})")
    w(" *")
    w(" * | Lua / console name | Enum | Declared at | Doc | Use sites |")
    w(" * |---|---|---|---|---|")
    for i in idents:
        d = declares[i]
        uses = "<br>".join(f"`{f}:{ln}` {k}" for _, f, ln, k in by_ident[i]) or "(none)"
        w(f" * | `{i.lower()}` | `TraitId::{i}` | `{d['file']}:{d['line']}` | {cell(d['doc'][1:-1])} | {uses} |")
    w(" *")
    w(f" * @section {page}_layout Generated layout")
    w(" *")
    w(f" * - `TraitId` : one enumerator per trait, sorted by identifier so numbering is stable.")
    w(f" * - `TraitBits` : one 1-bit field per trait on the entity (`e.traits.{idents[0].lower()}` ...), plus")
    w(" *   `get(TraitId)` / `set(TraitId, bool)` switches.")
    w(" * - `TRAIT_INFO[]` : name, doc and the declaring source line per trait, for runtime panels and `traits()`.")
    w(" * - `TRAIT_SITES[]` : every `HAS` / `ADD` / `RM` site, so tooling can show where a trait is used.")
    w(" * - `trait_name()` / `trait_from_name()` : Lua-name lookups used by `trait_add` and the console.")
    w(" *")
    w(" * Every generated line in the header ends with a `// DECLARE_TRAIT <file>:<line>` comment naming the")
    w(" * tag that produced it. All symbols are collected in @ref " + group + ".")
    w(" */")
    w("")
    w("/**")
    w(f" * @defgroup {group} Generated trait API: {src_name}")
    w(f" * @brief Symbols emitted by tools/gen_traits.py. Described on @ref {page}.")
    w(" * @{")
    w(" */")
    w("")
    w("// Every generated line below ends with the DECLARE_TRAIT tag (file:line) that produced it.")
    w("/// @brief One enumerator per DECLARE_TRAIT, sorted by identifier. `COUNT` is the trait count.")
    w("enum class TraitId : uint8_t {")
    for i in idents:
        d = declares[i]
        w(f"  {i}, // DECLARE_TRAIT {d['file']}:{d['line']}")
    w("  COUNT")
    w("};")
    w("/// @brief Number of generated traits.")
    w("constexpr int TRAIT_COUNT = (int)TraitId::COUNT;")
    w("")
    w("/// @brief One bit per trait. The trailing comment on each field is the DECLARE_TRAIT that generated")
    w("/// it; the doc lines above list the HAS/ADD/RM sites that use it.")
    w("struct TraitBits {")
    for i in idents:
        d = declares[i]
        w(f"  /// {i.lower()}: {d['doc'][1:-1]}")
        uses = ", ".join(f"{f}:{ln} {k}" for _, f, ln, k in by_ident[i])
        if uses:
            w(f"  ///   used {uses}")
        w(f"  uint8_t {i.lower()} : 1 = 0; // DECLARE_TRAIT {d['file']}:{d['line']}")
    w("")
    w("  bool get(TraitId id) const {")
    w("    switch (id) {")
    for i in idents:
        w(f"    case TraitId::{i}: return {i.lower()}; // DECLARE_TRAIT {declares[i]['file']}:{declares[i]['line']}")
    w("    default: return false;")
    w("    }")
    w("  }")
    w("  void set(TraitId id, bool v) {")
    w("    switch (id) {")
    for i in idents:
        w(f"    case TraitId::{i}: {i.lower()} = v; break; // DECLARE_TRAIT {declares[i]['file']}:{declares[i]['line']}")
    w("    default: break;")
    w("    }")
    w("  }")
    w("};")
    w("")
    w("/// @brief Static description of one trait: names, doc and the DECLARE_TRAIT that generated it.")
    w("struct TraitInfo {")
    w("  TraitId id;")
    w("  const char* ident; // enum spelling, e.g. THREE_POINT_LIT")
    w("  const char* name;  // Lua/console spelling, e.g. three_point_lit")
    w("  const char* doc;")
    w("  const char* file;  // where DECLARE_TRAIT lives")
    w("  int line;")
    w("};")
    w("/// @brief One row per trait, indexed by `(int)TraitId`.")
    w(f"inline constexpr TraitInfo TRAIT_INFO[TRAIT_COUNT] = {{")
    for i in idents:
        d = declares[i]
        w(f"    {{TraitId::{i}, {cstr(i)}, {cstr(i.lower())}, {d['doc']}, {cstr(d['file'])}, {d['line']}}}, // DECLARE_TRAIT {d['file']}:{d['line']}")
    w("};")
    w("")
    w("/// @brief One HAS_TRAIT / ADD_TRAIT / RM_TRAIT occurrence in the sources.")
    w("struct TraitSite {")
    w("  TraitId id;")
    w("  const char* file;")
    w("  int line;")
    w('  const char* kind; // "HAS", "ADD" or "RM"')
    w("};")
    w("/// @brief Every use site, grouped by trait in TraitId order.")
    w(f"inline constexpr TraitSite TRAIT_SITES[{max(len(sites), 1)}] = {{")
    for i in idents:
        for _, f, ln, k in by_ident[i]:
            w(f"    {{TraitId::{i}, {cstr(f)}, {ln}, {cstr(k)}}}, // {k}_TRAIT {f}:{ln}")
    if not sites:
        w("    {TraitId::COUNT, \"\", 0, \"\"},")
    w("};")
    w("/// @brief Number of entries in TRAIT_SITES.")
    w(f"constexpr int TRAIT_SITE_COUNT = {len(sites)};")
    w("")
    w("/// @brief Lua/console name of a trait (`three_point_lit` for `TraitId::THREE_POINT_LIT`).")
    w("constexpr const char* trait_name(TraitId id) { return TRAIT_INFO[(int)id].name; }")
    w("/// @brief Full TraitInfo row for a trait.")
    w("constexpr const TraitInfo& trait_info(TraitId id) { return TRAIT_INFO[(int)id]; }")
    w("")
    w("/// @brief Lua/console name -> index into TraitId, or -1 if unknown.")
    w("inline int trait_from_name(const char* s) {")
    w("  for (int i = 0; i < TRAIT_COUNT; i++)")
    w("    if (std::strcmp(TRAIT_INFO[i].name, s) == 0)")
    w("      return i;")
    w("  return -1;")
    w("}")
    w("")
    w("/** @} */ // end of group " + group)
    w("")
    return "\n".join(L)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--src", required=True, help="game source directory to scan")
    ap.add_argument("--out", required=True, help="header to write")
    a = ap.parse_args()
    src = os.path.abspath(a.src)
    if not os.path.isdir(src):
        print(f"gen_traits: error: --src {src} is not a directory", file=sys.stderr)
        return 2
    declares, sites, errors = scan(src)
    if errors:
        for e in errors:
            print(e, file=sys.stderr)
        print(f"gen_traits: {len(errors)} error(s), {a.out} not written", file=sys.stderr)
        return 1
    text = emit(declares, sites, os.path.basename(src))
    os.makedirs(os.path.dirname(os.path.abspath(a.out)) or ".", exist_ok=True)
    with open(a.out, "w", encoding="utf-8") as f:
        f.write(text)
    print(f"gen_traits: {len(declares)} traits, {len(sites)} sites -> {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
