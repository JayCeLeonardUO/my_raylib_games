#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>

EM_JS(int, get_file_count, (), {
    if (typeof window.__VAULT_FILES === "undefined") return 0;
    return window.__VAULT_FILES.length;
});

EM_JS(void, get_file_path, (int index, char *buf, int bufSize), {
    if (typeof window.__VAULT_FILES === "undefined") return;
    var arr = window.__VAULT_FILES;
    if (index < 0 || index >= arr.length) return;
    var str = arr[index].path || "";
    stringToUTF8(str, buf, bufSize);
});

EM_JS(void, get_html_dir, (char *buf, int bufSize), {
    var p = (typeof window.__HTML_PATH !== "undefined") ? window.__HTML_PATH : "";
    var slash = p.lastIndexOf("/");
    var dir = (slash >= 0) ? p.substring(0, slash) : "";
    stringToUTF8(dir, buf, bufSize);
});
#endif

static char filePaths[512][256];
static int fileCount = 0;
static char htmlDir[256] = "";
static char filterBuf[128] = "";

void UpdateAndDraw() {
    BeginDrawing();
    ClearBackground(DARKGRAY);

    rlImGuiBegin();

    ImGui::Begin("Vault Files");

    ImGui::Text("Directory: %s", htmlDir[0] ? htmlDir : "(root)");
    ImGui::Text("Files: %d", fileCount);
    ImGui::Separator();

    ImGui::InputText("Filter", filterBuf, sizeof(filterBuf));
    ImGui::Separator();

    if (fileCount == 0) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "No files found in this directory.");
    } else {
        ImGui::BeginChild("FileList", ImVec2(0, 0), true);
        for (int i = 0; i < fileCount; i++) {
            if (filterBuf[0] && !strstr(filePaths[i], filterBuf)) continue;
            ImGui::Selectable(filePaths[i]);
        }
        ImGui::EndChild();
    }

    ImGui::End();

    rlImGuiEnd();
    EndDrawing();
}

int main() {
#ifdef __EMSCRIPTEN__
    get_html_dir(htmlDir, 256);

    int total = get_file_count();
    int dirLen = (int)strlen(htmlDir);

    for (int i = 0; i < total && fileCount < 512; i++) {
        char fullPath[256] = {0};
        get_file_path(i, fullPath, 256);

        bool match = false;
        if (dirLen == 0) {
            match = (strchr(fullPath, '/') == NULL);
        } else if (strncmp(fullPath, htmlDir, dirLen) == 0 && fullPath[dirLen] == '/') {
            match = (strchr(fullPath + dirLen + 1, '/') == NULL);
        }

        if (match) {
            const char *name = strrchr(fullPath, '/');
            name = name ? name + 1 : fullPath;
            strncpy(filePaths[fileCount], name, 255);
            fileCount++;
        }
    }
#endif

    InitWindow(800, 600, "vault files");
    SetTargetFPS(60);
    rlImGuiSetup(true);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(UpdateAndDraw, 0, 1);
#else
    while (!WindowShouldClose()) {
        UpdateAndDraw();
    }
#endif

    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
