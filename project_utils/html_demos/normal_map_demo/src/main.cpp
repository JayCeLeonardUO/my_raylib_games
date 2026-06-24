#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <cmath>
#include <cstdio>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "imgui.h"
#include "rlImGui.h"

// ---------------------------------------------------------------------------
// Embedded shaders (GLSL 330)
// ---------------------------------------------------------------------------

#ifdef __EMSCRIPTEN__
#define GLSL_HEADER "#version 100\nprecision mediump float;\n"
#define IN_ATTR "attribute"
#define IN_VARY_VS "varying"
#define IN_VARY_FS "varying"
#define OUT_VARY "varying"
#define FRAG_OUT ""
#define FRAG_COLOR "gl_FragColor"
#define TEX2D "texture2D"
#else
#define GLSL_HEADER "#version 330\n"
#define IN_ATTR "in"
#define IN_VARY_VS "out"
#define IN_VARY_FS "in"
#define OUT_VARY "out"
#define FRAG_OUT "out vec4 finalColor;\n"
#define FRAG_COLOR "finalColor"
#define TEX2D "texture"
#endif

static const char *vertSrc =
    GLSL_HEADER
    IN_ATTR " vec3 vertexPosition;\n"
    IN_ATTR " vec2 vertexTexCoord;\n"
    IN_ATTR " vec3 vertexNormal;\n"
    "uniform mat4 mvp;\n"
    "uniform mat4 matModel;\n"
    "uniform mat4 matNormal;\n"
    IN_VARY_VS " vec3 fragPos;\n"
    IN_VARY_VS " vec2 fragUV;\n"
    IN_VARY_VS " vec3 fragNormal;\n"
    "void main() {\n"
    "    fragPos    = vec3(matModel * vec4(vertexPosition, 1.0));\n"
    "    fragUV     = vertexTexCoord;\n"
    "    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));\n"
    "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";

static const char *fragSrc =
    GLSL_HEADER
    IN_VARY_FS " vec3 fragPos;\n"
    IN_VARY_FS " vec2 fragUV;\n"
    IN_VARY_FS " vec3 fragNormal;\n"
    "uniform sampler2D texture0;\n"   // diffuse
    "uniform sampler2D texture1;\n"   // normal map
    "uniform sampler2D texture2;\n"   // height map
    "uniform vec2  texScale;\n"
    "uniform int   useNormalMap;\n"
    "uniform int   usePOM;\n"
    "uniform float pomScale;\n"
    "uniform float pomMinLayersF;\n"
    "uniform float pomMaxLayersF;\n"
    "uniform vec3  lightPos;\n"
    "uniform vec3  viewPos;\n"
    "uniform vec3  lightColor;\n"
    "uniform float ambientStrength;\n"
    "uniform float specularStrength;\n"
    "uniform float specularPower;\n"
    FRAG_OUT
    "void main() {\n"
    "    vec2 uv = fragUV * texScale;\n"
    "    vec3 N = normalize(fragNormal);\n"
    "    vec3 geoN = N;\n"
    "    vec3 V = normalize(viewPos - fragPos);\n"
    // POM
    "    if (usePOM != 0) {\n"
    "        vec3 tT = vec3(1.0, 0.0, 0.0);\n"
    "        vec3 tB = vec3(0.0, 0.0, 1.0);\n"
    "        vec3 vdt = normalize(vec3(dot(V, tT), dot(V, tB), dot(V, geoN)));\n"
    "        float numLayers = mix(pomMaxLayersF, pomMinLayersF, max(dot(vec3(0.0, 0.0, 1.0), vdt), 0.0));\n"
    "        numLayers = max(numLayers, 1.0);\n"
    "        float layerDepth = 1.0 / numLayers;\n"
    "        float curLD = 0.0;\n"
    "        vec2 dUV = (vdt.xy / vdt.z) * pomScale * layerDepth;\n"
    "        vec2 curUV = uv;\n"
    "        float curH = 1.0 - " TEX2D "(texture2, curUV).r;\n"
    "        for (int i = 0; i < 128; i++) {\n"
    "            if (curLD >= curH) break;\n"
    "            curUV -= dUV;\n"
    "            curH = 1.0 - " TEX2D "(texture2, curUV).r;\n"
    "            curLD += layerDepth;\n"
    "        }\n"
    "        vec2 prevUV = curUV + dUV;\n"
    "        float aft = curH - curLD;\n"
    "        float bef = (1.0 - " TEX2D "(texture2, prevUV).r) - (curLD - layerDepth);\n"
    "        float w = aft / (aft - bef);\n"
    "        uv = mix(curUV, prevUV, w);\n"
    "    }\n"
    // Normal map
    "    if (useNormalMap != 0) {\n"
    "        vec3 T = vec3(1.0, 0.0, 0.0);\n"
    "        vec3 B = vec3(0.0, 0.0, 1.0);\n"
    "        vec3 mapN = " TEX2D "(texture1, uv).rgb * 2.0 - 1.0;\n"
    "        N = normalize(T * mapN.x + geoN * mapN.y + B * mapN.z);\n"
    "    }\n"
    // Lighting
    "    vec3 albedo = " TEX2D "(texture0, uv).rgb;\n"
    "    vec3  L    = lightPos - fragPos;\n"
    "    float dist = length(L);\n"
    "    L = normalize(L);\n"
    "    float atten = 1.0 / (1.0 + 0.045 * dist + 0.0075 * dist * dist);\n"
    "    vec3  H = normalize(L + V);\n"
    "    float diff = max(dot(N, L), 0.0);\n"
    "    float spec = pow(max(dot(N, H), 0.0), specularPower);\n"
    "    vec3 ambient  = ambientStrength * lightColor * albedo;\n"
    "    vec3 diffuse  = diff * lightColor * albedo * atten;\n"
    "    vec3 specular = specularStrength * spec * lightColor * atten;\n"
    "    " FRAG_COLOR " = vec4(ambient + diffuse + specular, 1.0);\n"
    "}\n";

// ---------------------------------------------------------------------------
// Procedural texture generation
// ---------------------------------------------------------------------------

struct BrickParams {
    int texW = 512, texH = 512;
    int brickW = 60, brickH = 28;
    int mortar = 6;
};

static void GenBrickDiffuse(Image *img, const BrickParams &p)
{
    *img = GenImageColor(p.texW, p.texH, Color{55, 55, 55, 255}); // mortar
    Color *px = (Color *)img->data;

    for (int y = 0; y < p.texH; y++) {
        int row = y / (p.brickH + p.mortar);
        int yInCell = y % (p.brickH + p.mortar);
        bool inMortarY = yInCell >= p.brickH;

        int xOffset = (row % 2) * ((p.brickW + p.mortar) / 2);

        for (int x = 0; x < p.texW; x++) {
            int xShifted = (x + xOffset) % p.texW;
            int xInCell = xShifted % (p.brickW + p.mortar);
            bool inMortarX = xInCell >= p.brickW;

            if (!inMortarX && !inMortarY) {
                // Per-brick color variation based on row+col hash
                int col = xShifted / (p.brickW + p.mortar);
                unsigned int hash = (unsigned int)(row * 137 + col * 59);
                int rv = (int)(hash % 30) - 15;
                int r = 155 + rv;
                int g = 75  + rv / 2;
                int b = 55  + rv / 3;
                px[y * p.texW + x] = Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
            }
        }
    }
}

static void GenBrickHeightMap(Image *img, const BrickParams &p)
{
    *img = GenImageColor(p.texW, p.texH, Color{0, 0, 0, 255}); // mortar = black
    Color *px = (Color *)img->data;
    int inset = 2;

    for (int y = 0; y < p.texH; y++) {
        int row = y / (p.brickH + p.mortar);
        int yInCell = y % (p.brickH + p.mortar);
        bool inMortarY = yInCell >= p.brickH;

        int xOffset = (row % 2) * ((p.brickW + p.mortar) / 2);

        for (int x = 0; x < p.texW; x++) {
            int xShifted = (x + xOffset) % p.texW;
            int xInCell = xShifted % (p.brickW + p.mortar);
            bool inMortarX = xInCell >= p.brickW;

            if (!inMortarX && !inMortarY) {
                // Inset for bevel
                bool edge = xInCell < inset || xInCell >= p.brickW - inset
                         || yInCell < inset || yInCell >= p.brickH - inset;
                unsigned char v = edge ? 160 : 200;
                px[y * p.texW + x] = Color{v, v, v, 255};
            }
        }
    }

    // 3x3 box blur
    Color *tmp = (Color *)RL_MALLOC(p.texW * p.texH * sizeof(Color));
    for (int y = 0; y < p.texH; y++) {
        for (int x = 0; x < p.texW; x++) {
            int sum = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int sx = (x + dx + p.texW) % p.texW;
                    int sy = (y + dy + p.texH) % p.texH;
                    sum += px[sy * p.texW + sx].r;
                }
            }
            unsigned char avg = (unsigned char)(sum / 9);
            tmp[y * p.texW + x] = Color{avg, avg, avg, 255};
        }
    }
    memcpy(px, tmp, p.texW * p.texH * sizeof(Color));
    RL_FREE(tmp);
}

static void GenNormalMapFromHeight(Image *normalImg, const Image &heightImg, float strength)
{
    int w = heightImg.width, h = heightImg.height;
    *normalImg = GenImageColor(w, h, Color{128, 128, 255, 255});
    Color *dst = (Color *)normalImg->data;
    Color *src = (Color *)heightImg.data;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float hL = src[y * w + ((x - 1 + w) % w)].r / 255.0f;
            float hR = src[y * w + ((x + 1) % w)].r / 255.0f;
            float hD = src[((y + 1) % h) * w + x].r / 255.0f;
            float hU = src[((y - 1 + h) % h) * w + x].r / 255.0f;

            float dx = (hL - hR) * strength;
            float dy = (hD - hU) * strength;
            float dz = 1.0f;

            // Normalize
            float len = sqrtf(dx * dx + dy * dy + dz * dz);
            dx /= len; dy /= len; dz /= len;

            // Encode [-1,1] -> [0,255]
            dst[y * w + x] = Color{
                (unsigned char)((dx * 0.5f + 0.5f) * 255.0f),
                (unsigned char)((dy * 0.5f + 0.5f) * 255.0f),
                (unsigned char)((dz * 0.5f + 0.5f) * 255.0f),
                255
            };
        }
    }
}

// ---------------------------------------------------------------------------
// App state (global for Emscripten callback compatibility)
// ---------------------------------------------------------------------------
static struct AppState {
    Camera3D camera;
    Model planeModel;
    Shader shader;
    Image heightImg;
    float *origVerts;
    int vertCount;

    Texture2D diffPreview, normalPreview, heightPreview;

    // Shader uniform locations
    int locTexScale, locUseNormal, locLightPos, locViewPos;
    int locLightColor, locAmbient, locSpecStr, locSpecPow;
    int locUsePOM, locPomScale, locPomMinLayers, locPomMaxLayers;

    // UI state
    bool enableNormal  = true;
    bool autoRotate    = true;
    float rotateSpeed  = 1.0f;
    float lightHeight  = 2.0f;
    float lightRadius  = 3.5f;
    float lightAngleDeg = 0.0f;
    float lightColor[3] = {1.0f, 0.95f, 0.85f};

    float ambient   = 0.12f;
    float specStr   = 0.6f;
    float shininess = 32.0f;
    float texScale  = 2.0f;

    bool enableHeightMap = false;
    float displacementStr = 0.3f;

    bool enablePOM   = false;
    float pomScale   = 0.05f;
    int pomMinLayers = 8;
    int pomMaxLayers = 32;

    float prevDisplacement = -1.0f;
    float prevTexScale     = -1.0f;
    bool  prevHeightEnabled = false;

    int previewMode = 0;
    float lightAngle = 0.0f;
} app;

// ---------------------------------------------------------------------------
// Frame update + draw
// ---------------------------------------------------------------------------
static void UpdateAndDraw()
{
    float dt = GetFrameTime();

    // Light orbit
    if (app.autoRotate) {
        app.lightAngle += app.rotateSpeed * dt;
        app.lightAngleDeg = app.lightAngle * RAD2DEG;
    } else {
        app.lightAngle = app.lightAngleDeg * DEG2RAD;
    }
    Vector3 lightPos = {
        cosf(app.lightAngle) * app.lightRadius,
        app.lightHeight,
        sinf(app.lightAngle) * app.lightRadius
    };

    // Camera (skip when ImGui wants mouse)
    if (!ImGui::GetIO().WantCaptureMouse) {
        UpdateCamera(&app.camera, CAMERA_ORBITAL);
    }

    // --- Height map displacement ---
    Mesh &mesh = app.planeModel.meshes[0];
    if (app.enableHeightMap != app.prevHeightEnabled ||
        (app.enableHeightMap && (app.displacementStr != app.prevDisplacement || app.texScale != app.prevTexScale))) {
        Color *hpx = (Color *)app.heightImg.data;
        float planeW = 6.0f, planeD = 6.0f;

        for (int i = 0; i < app.vertCount; i++) {
            float ox = app.origVerts[i * 3 + 0];
            float oy = app.origVerts[i * 3 + 1];
            float oz = app.origVerts[i * 3 + 2];

            if (app.enableHeightMap) {
                float u = (ox / planeW + 0.5f) * app.texScale;
                float v = (oz / planeD + 0.5f) * app.texScale;
                int tx = ((int)(u * app.heightImg.width) % app.heightImg.width + app.heightImg.width) % app.heightImg.width;
                int ty = ((int)(v * app.heightImg.height) % app.heightImg.height + app.heightImg.height) % app.heightImg.height;
                float h = hpx[ty * app.heightImg.width + tx].r / 255.0f;
                mesh.vertices[i * 3 + 0] = ox;
                mesh.vertices[i * 3 + 1] = oy + h * app.displacementStr;
                mesh.vertices[i * 3 + 2] = oz;
            } else {
                mesh.vertices[i * 3 + 0] = ox;
                mesh.vertices[i * 3 + 1] = oy;
                mesh.vertices[i * 3 + 2] = oz;
            }
        }
        UpdateMeshBuffer(mesh, 0, mesh.vertices, app.vertCount * 3 * sizeof(float), 0);
        app.prevHeightEnabled = app.enableHeightMap;
        app.prevDisplacement = app.displacementStr;
        app.prevTexScale = app.texScale;
    }

    // --- Set shader uniforms ---
    float texScaleV[2] = {app.texScale, app.texScale};
    int useNM = app.enableNormal ? 1 : 0;
    SetShaderValue(app.shader, app.locTexScale,   texScaleV,             SHADER_UNIFORM_VEC2);
    SetShaderValue(app.shader, app.locUseNormal,  &useNM,               SHADER_UNIFORM_INT);
    SetShaderValue(app.shader, app.locLightPos,   &lightPos,            SHADER_UNIFORM_VEC3);
    SetShaderValue(app.shader, app.locViewPos,    &app.camera.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(app.shader, app.locLightColor, app.lightColor,       SHADER_UNIFORM_VEC3);
    SetShaderValue(app.shader, app.locAmbient,    &app.ambient,         SHADER_UNIFORM_FLOAT);
    SetShaderValue(app.shader, app.locSpecStr,    &app.specStr,         SHADER_UNIFORM_FLOAT);
    SetShaderValue(app.shader, app.locSpecPow,    &app.shininess,       SHADER_UNIFORM_FLOAT);

    int usePOMi = app.enablePOM ? 1 : 0;
    float pomMinF = (float)app.pomMinLayers;
    float pomMaxF = (float)app.pomMaxLayers;
    SetShaderValue(app.shader, app.locUsePOM,       &usePOMi,       SHADER_UNIFORM_INT);
    SetShaderValue(app.shader, app.locPomScale,     &app.pomScale,  SHADER_UNIFORM_FLOAT);
    SetShaderValue(app.shader, app.locPomMinLayers, &pomMinF,       SHADER_UNIFORM_FLOAT);
    SetShaderValue(app.shader, app.locPomMaxLayers, &pomMaxF,       SHADER_UNIFORM_FLOAT);

    // --- Draw ---
    BeginDrawing();
    ClearBackground(Color{30, 30, 35, 255});

    BeginMode3D(app.camera);

    DrawModel(app.planeModel, Vector3{0, 0, 0}, 1.0f, WHITE);

    Color lightSphereColor = {
        (unsigned char)(app.lightColor[0] * 255),
        (unsigned char)(app.lightColor[1] * 255),
        (unsigned char)(app.lightColor[2] * 255), 255
    };
    DrawSphere(lightPos, 0.12f, lightSphereColor);

    int dashes = 10;
    for (int i = 0; i < dashes; i++) {
        float t0 = (float)i / dashes;
        float t1 = (float)(i + 0.5f) / dashes;
        Vector3 a = {lightPos.x, lightPos.y * (1.0f - t0), lightPos.z};
        Vector3 b = {lightPos.x, lightPos.y * (1.0f - t1), lightPos.z};
        DrawLine3D(a, b, YELLOW);
    }

    EndMode3D();

    // --- ImGui ---
    rlImGuiBegin();

    ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_Once);

    if (ImGui::Begin("Normal Map Controls")) {
        ImGui::TextWrapped(
            "A flat plane lit by a point light. Toggle the normal map to see "
            "how it fakes surface detail without adding geometry.");
        ImGui::Separator();

        ImGui::Checkbox("Enable Normal Map", &app.enableNormal);
        if (app.enableNormal) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f),
                "Normal map: ON — notice the bumps and mortar grooves");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                "Normal map: OFF — perfectly flat, no surface detail");
        }

        ImGui::Spacing();

        ImGui::Checkbox("Enable Height Map", &app.enableHeightMap);
        if (app.enableHeightMap) {
            ImGui::SliderFloat("Displacement", &app.displacementStr, 0.01f, 1.0f);
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f),
                "Height map: ON — geometry is deformed");
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "Height map: OFF — flat geometry");
        }

        ImGui::Spacing();

        ImGui::Checkbox("Enable POM", &app.enablePOM);
        if (app.enablePOM) {
            ImGui::SliderFloat("POM Depth", &app.pomScale, 0.01f, 0.15f);
            ImGui::SliderInt("Min Layers", &app.pomMinLayers, 4, 64);
            ImGui::SliderInt("Max Layers", &app.pomMaxLayers, 16, 128);
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                "POM: ON — view-dependent depth illusion");
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "POM: OFF — no parallax occlusion");
        }

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Auto-Rotate", &app.autoRotate);
            if (app.autoRotate) {
                ImGui::SliderFloat("Rotate Speed", &app.rotateSpeed, 0.1f, 3.0f);
            } else {
                ImGui::SliderAngle("Light Angle", &app.lightAngle);
                app.lightAngleDeg = app.lightAngle * RAD2DEG;
            }
            ImGui::SliderFloat("Light Height", &app.lightHeight, 0.3f, 6.0f);
            ImGui::SliderFloat("Light Radius", &app.lightRadius, 1.0f, 8.0f);
            ImGui::ColorEdit3("Light Color", app.lightColor);
        }

        if (ImGui::CollapsingHeader("Material")) {
            ImGui::SliderFloat("Ambient",   &app.ambient,   0.0f, 0.5f);
            ImGui::SliderFloat("Specular",  &app.specStr,   0.0f, 2.0f);
            ImGui::SliderFloat("Shininess", &app.shininess,  2.0f, 128.0f);
            ImGui::SliderFloat("Tex Scale", &app.texScale,   0.5f, 6.0f);
        }

        if (ImGui::CollapsingHeader("Texture Preview")) {
            ImGui::RadioButton("None",    &app.previewMode, 0); ImGui::SameLine();
            ImGui::RadioButton("Diffuse", &app.previewMode, 1); ImGui::SameLine();
            ImGui::RadioButton("Normal",  &app.previewMode, 2); ImGui::SameLine();
            ImGui::RadioButton("Height",  &app.previewMode, 3);

            if (app.previewMode == 1) rlImGuiImage(&app.diffPreview);
            else if (app.previewMode == 2) rlImGuiImage(&app.normalPreview);
            else if (app.previewMode == 3) rlImGuiImage(&app.heightPreview);
        }
    }
    ImGui::End();

    rlImGuiEnd();

    DrawFPS(GetScreenWidth() - 90, GetScreenHeight() - 30);

    EndDrawing();
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main()
{
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 800, "Normal Map Demo");
    SetTargetFPS(60);

    rlImGuiSetup(true);

    // --- Camera ---
    app.camera = {};
    app.camera.position = {3.0f, 4.0f, 3.0f};
    app.camera.target   = {0.0f, 0.0f, 0.0f};
    app.camera.up        = {0.0f, 1.0f, 0.0f};
    app.camera.fovy      = 45.0f;
    app.camera.projection = CAMERA_PERSPECTIVE;

    // --- Plane mesh ---
    Mesh planeMesh = GenMeshPlane(6.0f, 6.0f, 128, 128);
    app.planeModel = LoadModelFromMesh(planeMesh);

    Mesh &mesh = app.planeModel.meshes[0];
    app.vertCount = mesh.vertexCount;
    app.origVerts = (float *)RL_MALLOC(app.vertCount * 3 * sizeof(float));
    memcpy(app.origVerts, mesh.vertices, app.vertCount * 3 * sizeof(float));

    // --- Procedural textures ---
    BrickParams bp;
    Image diffImg, normalImg;
    GenBrickDiffuse(&diffImg, bp);
    GenBrickHeightMap(&app.heightImg, bp);
    GenNormalMapFromHeight(&normalImg, app.heightImg, 3.0f);

    Texture2D diffTex   = LoadTextureFromImage(diffImg);
    Texture2D normalTex = LoadTextureFromImage(normalImg);
    Texture2D heightTex = LoadTextureFromImage(app.heightImg);

    app.diffPreview   = LoadTextureFromImage(diffImg);
    app.normalPreview = LoadTextureFromImage(normalImg);
    app.heightPreview = LoadTextureFromImage(app.heightImg);

    UnloadImage(diffImg);
    UnloadImage(normalImg);

    SetTextureFilter(diffTex,   TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(normalTex, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(diffTex,   TEXTURE_WRAP_REPEAT);
    SetTextureWrap(normalTex, TEXTURE_WRAP_REPEAT);
    SetTextureFilter(heightTex, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(heightTex,   TEXTURE_WRAP_REPEAT);

    // --- Shader ---
    app.shader = LoadShaderFromMemory(vertSrc, fragSrc);

    app.locTexScale   = GetShaderLocation(app.shader, "texScale");
    app.locUseNormal  = GetShaderLocation(app.shader, "useNormalMap");
    app.locLightPos   = GetShaderLocation(app.shader, "lightPos");
    app.locViewPos    = GetShaderLocation(app.shader, "viewPos");
    app.locLightColor = GetShaderLocation(app.shader, "lightColor");
    app.locAmbient    = GetShaderLocation(app.shader, "ambientStrength");
    app.locSpecStr    = GetShaderLocation(app.shader, "specularStrength");
    app.locSpecPow    = GetShaderLocation(app.shader, "specularPower");

    app.locUsePOM       = GetShaderLocation(app.shader, "usePOM");
    app.locPomScale     = GetShaderLocation(app.shader, "pomScale");
    app.locPomMinLayers = GetShaderLocation(app.shader, "pomMinLayersF");
    app.locPomMaxLayers = GetShaderLocation(app.shader, "pomMaxLayersF");

    app.shader.locs[SHADER_LOC_MAP_NORMAL]  = GetShaderLocation(app.shader, "texture1");
    app.shader.locs[SHADER_LOC_MAP_HEIGHT]  = GetShaderLocation(app.shader, "texture2");
    app.shader.locs[SHADER_LOC_VECTOR_VIEW] = app.locViewPos;

    int tex0 = 0, tex1 = 1;
    SetShaderValue(app.shader, GetShaderLocation(app.shader, "texture0"), &tex0, SHADER_UNIFORM_INT);
    SetShaderValue(app.shader, GetShaderLocation(app.shader, "texture1"), &tex1, SHADER_UNIFORM_INT);

    // Assign to model
    app.planeModel.materials[0].shader = app.shader;
    app.planeModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = diffTex;
    app.planeModel.materials[0].maps[MATERIAL_MAP_NORMAL].texture  = normalTex;
    app.planeModel.materials[0].maps[MATERIAL_MAP_HEIGHT].texture  = heightTex;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(UpdateAndDraw, 0, 1);
#else
    while (!WindowShouldClose()) {
        UpdateAndDraw();
    }
#endif

    // Cleanup
    RL_FREE(app.origVerts);
    UnloadImage(app.heightImg);
    rlImGuiShutdown();

    UnloadTexture(app.diffPreview);
    UnloadTexture(app.normalPreview);
    UnloadTexture(app.heightPreview);

    UnloadShader(app.shader);
    UnloadModel(app.planeModel);

    CloseWindow();
    return 0;
}
