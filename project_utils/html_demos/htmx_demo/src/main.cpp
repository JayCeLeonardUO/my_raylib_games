#include <raylib.h>
#include <raymath.h>
#include <cmath>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

static float angle = 0.0f;
static bool spinning = true;
static int colorId = 1; // 1=RED, 2=GREEN, 3=BLUE
static Camera3D camera = {0};

// --- Exported functions: JS calls these via Module._js_* ---

#ifdef __EMSCRIPTEN__
extern "C" {

EMSCRIPTEN_KEEPALIVE
void js_set_color(int id) {
    if (id >= 1 && id <= 3) colorId = id;
}

EMSCRIPTEN_KEEPALIVE
void js_toggle_spin() {
    spinning = !spinning;
}

EMSCRIPTEN_KEEPALIVE
float js_get_angle() {
    return angle;
}

EMSCRIPTEN_KEEPALIVE
int js_get_color() {
    return colorId;
}

} // extern "C"

// C -> JS: push a message to the HTML status bar
EM_JS(void, notify_html, (const char *msg), {
    if (typeof window.updateStatusFromC === 'function') {
        window.updateStatusFromC(UTF8ToString(msg));
    }
});
#else
void notify_html(const char *) {}
#endif

Color GetCubeColor() {
    switch (colorId) {
        case 1: return RED;
        case 2: return GREEN;
        case 3: return BLUE;
        default: return WHITE;
    }
}

void UpdateAndDraw() {
    if (spinning) angle += 0.02f;

    camera.position = (Vector3){
        cosf(angle) * 6.0f,
        4.0f,
        sinf(angle) * 6.0f
    };

    BeginDrawing();
    ClearBackground((Color){26, 26, 46, 255});

    BeginMode3D(camera);
    DrawCube((Vector3){0, 0, 0}, 2.0f, 2.0f, 2.0f, GetCubeColor());
    DrawCubeWires((Vector3){0, 0, 0}, 2.0f, 2.0f, 2.0f, WHITE);
    DrawGrid(10, 1.0f);
    EndMode3D();

    DrawText("HTML buttons control C code", 10, 10, 16, WHITE);
    DrawText(TextFormat("angle=%.1f spin=%s color=%d", angle, spinning ? "ON" : "OFF", colorId),
             10, 30, 16, GRAY);

    EndDrawing();
}

int main() {
    InitWindow(800, 600, "htmx + raylib");
    SetTargetFPS(60);

    camera.target = (Vector3){0, 0, 0};
    camera.up = (Vector3){0, 1, 0};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

#ifdef __EMSCRIPTEN__
    notify_html("Raylib initialized!");
    emscripten_set_main_loop(UpdateAndDraw, 0, 1);
#else
    while (!WindowShouldClose()) {
        UpdateAndDraw();
    }
#endif

    CloseWindow();
    return 0;
}
