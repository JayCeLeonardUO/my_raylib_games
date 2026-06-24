#include <raylib.h>
#include <raymath.h>
#include <math.h>
#include <stdio.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

static Model model = {0};
static bool modelLoaded = false;
static ModelAnimation *anims = NULL;
static int animCount = 0;
static int animFrame = 0;

static Camera3D camera = {0};
static float camAngle = 0.8f;
static float camPitch = 0.4f;
static float camDist = 8.0f;
static Vector3 camTarget = {0, 0, 0};
static bool orbiting = false;
static Vector2 lastMouse = {0};


void UpdateAndDraw(void)
{
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        orbiting = true;
        lastMouse = GetMousePosition();
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) orbiting = false;
    if (orbiting) {
        Vector2 cur = GetMousePosition();
        camAngle -= (cur.x - lastMouse.x) * 0.005f;
        camPitch += (cur.y - lastMouse.y) * 0.005f;
        if (camPitch < -1.4f) camPitch = -1.4f;
        if (camPitch > 1.4f) camPitch = 1.4f;
        lastMouse = cur;
    }
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        camDist -= wheel * 0.8f;
        if (camDist < 2.0f) camDist = 2.0f;
        if (camDist > 30.0f) camDist = 30.0f;
    }

    camera.position = (Vector3){
        camTarget.x + cosf(camAngle) * cosf(camPitch) * camDist,
        camTarget.y + sinf(camPitch) * camDist,
        camTarget.z + sinf(camAngle) * cosf(camPitch) * camDist
    };

    // Tick animation
    if (modelLoaded && animCount > 0) {
        animFrame++;
        for (int i = 0; i < animCount; i++) {
            int frame = animFrame % anims[i].frameCount;
            UpdateModelAnimation(model, anims[i], frame);
        }
    }

    // Draw
    BeginDrawing();
    ClearBackground((Color){25, 25, 35, 255});

    BeginMode3D(camera);
    DrawGrid(10, 1.0f);
    if (modelLoaded) {
        DrawModel(model, (Vector3){0, 0, 0}, 1.0f, WHITE);
    }
    EndMode3D();

    DrawText("GLB Bone Animation Demo", 10, 10, 18, WHITE);
    if (modelLoaded && animCount > 0) {
        DrawText(TextFormat("Anims: %d | Frame: %d", animCount, animFrame), 10, 34, 14, GRAY);
        DrawText(TextFormat("Meshes: %d | Bones: %d", model.meshCount, model.boneCount), 10, 52, 14, GRAY);
    } else if (modelLoaded) {
        DrawText("Model loaded but NO ANIMATIONS", 10, 34, 14, RED);
    } else {
        DrawText("No model loaded", 10, 34, 14, RED);
    }
    DrawText("Left-drag: orbit | Scroll: zoom", 10, GetScreenHeight() - 24, 14,
        (Color){150, 150, 150, 150});
    DrawFPS(GetScreenWidth() - 90, 10);

    EndDrawing();
}

int main(void)
{
    setbuf(stdout, NULL);  // unbuffered so we see prints before crash/kill
    InitWindow(800, 600, "GLB Anim Scale Demo");
    SetTargetFPS(60);

    camera.target = camTarget;
    camera.up = (Vector3){0, 1, 0};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    const char *glbPath = "test_cubes.glb";
    printf("[INIT] Checking for: %s\n", glbPath);
    printf("[INIT] FileExists: %d\n", FileExists(glbPath));

    if (FileExists(glbPath)) {
        model = LoadModel(glbPath);
        modelLoaded = true;

        anims = LoadModelAnimations(glbPath, &animCount);
        printf("[LOAD] meshes=%d bones=%d anims=%d\n",
            model.meshCount, model.boneCount, animCount);

        // Fit camera
        BoundingBox bb = GetModelBoundingBox(model);
        camTarget = (Vector3){
            (bb.min.x + bb.max.x) * 0.5f,
            (bb.min.y + bb.max.y) * 0.5f,
            (bb.min.z + bb.max.z) * 0.5f
        };
        float maxSize = fmaxf(fmaxf(bb.max.x - bb.min.x, bb.max.y - bb.min.y), bb.max.z - bb.min.z);
        camDist = maxSize * 2.5f;
        if (camDist < 3.0f) camDist = 3.0f;
        printf("[INIT] Camera: target=(%.2f,%.2f,%.2f) dist=%.2f\n",
            camTarget.x, camTarget.y, camTarget.z, camDist);
    } else {
        printf("[ERROR] File not found: %s\n", glbPath);
    }

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(UpdateAndDraw, 0, 1);
#else
    while (!WindowShouldClose()) {
        UpdateAndDraw();
    }
#endif

    if (animCount > 0) UnloadModelAnimations(anims, animCount);
    if (modelLoaded) UnloadModel(model);
    CloseWindow();
    return 0;
}
