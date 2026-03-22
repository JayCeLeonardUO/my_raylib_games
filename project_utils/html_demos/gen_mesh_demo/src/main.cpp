#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <cmath>
#include <cstdint>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// --- Heightmap config ---
static constexpr int MAP_SIZE = 64;
static constexpr float CELL_SIZE = 0.16f;
static constexpr float HEIGHT_SCALE = 4.0f;
static constexpr float GAP = 0.01f;
static constexpr float MESH_SCALE_XZ = MAP_SIZE * CELL_SIZE;

// --- LOD config ---
static constexpr float LOD_DIST[] = {2.0f, 4.0f, 6.0f};

// --- Tile types ---
static constexpr int TILE_COUNT = 5;
static Texture2D tileTex[TILE_COUNT] = {};
static Material tileMat[TILE_COUNT] = {};
static Mesh planeMesh = {0};

// --- Solid LOD mesh (pre-allocated, rebuilt each frame) ---
static constexpr int MAX_QUADS = 4096;
static constexpr int MAX_VERTS = MAX_QUADS * 6;
static Mesh solidMesh = {0};
static Model solidModel = {0};
static bool solidMeshInited = false;
static int solidVertCount = 0;

// --- Render mode: 0 = tiles, 1 = solid mesh ---
static int renderMode = 0;

static uint8_t heightmap[MAP_SIZE * MAP_SIZE] = {0};

static Camera3D camera = {0};
static float camAngle = 0.8f;
static float camPitch = 0.6f;
static float camDist = 8.0f;
static bool dragging = false;
static Vector2 lastMouse = {0};

static int drawCallCount = 0;

// --- Procedural noise ---
static uint8_t noise8(int x, int y, int seed) {
    uint32_t s = (uint32_t)(x + y * 997 + seed * 7919);
    s = s * 747796405u + 2891336453u;
    uint32_t w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (uint8_t)(((w >> 22u) ^ w) & 0xFF);
}

static Texture2D GenTile(int tileIdx) {
    constexpr int PX = 16;
    Image img = GenImageColor(PX, PX, BLANK);
    for (int y = 0; y < PX; y++) {
        for (int x = 0; x < PX; x++) {
            Color c = {0, 0, 0, 255};
            uint8_t n;
            switch (tileIdx) {
                case 0:
                    n = noise8(x,y,0)/8;
                    c = {(unsigned char)(30+n),(unsigned char)(60+(int)(15*sinf(x*0.9f+y*0.4f))+n),(unsigned char)(140+n),255};
                    break;
                case 1:
                    n = noise8(x,y,100)/4;
                    c = {(unsigned char)(180+n),(unsigned char)(160+n),(unsigned char)(100+n),255};
                    break;
                case 2:
                    n = noise8(x,y,200)/4;
                    c = {(unsigned char)(40+n),(unsigned char)(120+n+(noise8(x*3,y*3,201)>200?30:0)),(unsigned char)(30+n),255};
                    break;
                case 3:
                    n = noise8(x,y,300)/5;
                    { uint8_t cr=(noise8(x*2,y*2,301)>240)?30:0; uint8_t b=100+n-cr;
                      c = {(unsigned char)(b+10),b,(unsigned char)(b>5?b-5:0),255}; }
                    break;
                case 4:
                    n = noise8(x,y,400)/8;
                    { uint8_t b=(uint8_t)(210+n+(noise8(x*5,y*5,401)>245?15:0));
                      c = {b,b,(unsigned char)(b<250?b+5:255),255}; }
                    break;
            }
            ImageDrawPixel(&img, x, y, c);
        }
    }
    for (int i = 0; i < PX; i++) {
        Color edge = {0,0,0,80};
        ImageDrawPixel(&img,i,0,edge); ImageDrawPixel(&img,i,PX-1,edge);
        ImageDrawPixel(&img,0,i,edge); ImageDrawPixel(&img,PX-1,i,edge);
    }
    Texture2D tex = LoadTextureFromImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    UnloadImage(img);
    return tex;
}

static Mesh GenPlaneMesh() {
    Mesh m = {0};
    m.triangleCount = 2; m.vertexCount = 6;
    m.vertices  = (float *)MemAlloc(6*3*sizeof(float));
    m.normals   = (float *)MemAlloc(6*3*sizeof(float));
    m.texcoords = (float *)MemAlloc(6*2*sizeof(float));
    float h = 0.5f;
    float verts[] = {-h,0,-h, -h,0,+h, +h,0,-h, +h,0,-h, -h,0,+h, +h,0,+h};
    float norms[] = {0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0};
    float uvs[] = {0,0, 0,1, 1,0, 1,0, 0,1, 1,1};
    memcpy(m.vertices, verts, sizeof(verts));
    memcpy(m.normals, norms, sizeof(norms));
    memcpy(m.texcoords, uvs, sizeof(uvs));
    UploadMesh(&m, false);
    return m;
}

static int TileFromHeight(uint8_t h) {
    float t = h / 255.0f;
    if (t < 0.08f) return 0;
    if (t < 0.20f) return 1;
    if (t < 0.55f) return 2;
    if (t < 0.80f) return 3;
    return 4;
}

static Color HeightColor(float y) {
    float t = y / HEIGHT_SCALE;
    if (t < 0.05f) return {40,80,160,255};
    if (t < 0.3f) { float s=(t-0.05f)/0.25f; return {(unsigned char)(40+s*60),(unsigned char)(120+s*60),40,255}; }
    if (t < 0.7f) { float s=(t-0.3f)/0.4f; return {(unsigned char)(100+s*60),(unsigned char)(80+s*20),(unsigned char)(40+s*20),255}; }
    float s = (t-0.7f)/0.3f;
    return {(unsigned char)(160+s*95),(unsigned char)(160+s*95),(unsigned char)(160+s*95),255};
}

// ===================== TILE LOD RENDERING =====================

static float AverageBlockHeight(int bx, int by, int blockSize) {
    float sum = 0; int count = 0;
    for (int dy = 0; dy < blockSize && (by+dy) < MAP_SIZE; dy++)
        for (int dx = 0; dx < blockSize && (bx+dx) < MAP_SIZE; dx++) {
            sum += heightmap[(by+dy)*MAP_SIZE+(bx+dx)]; count++;
        }
    return count > 0 ? sum/count : 0;
}

static void DrawTile(int bx, int by, int blockSize, float halfGrid) {
    float avgH = AverageBlockHeight(bx, by, blockSize);
    float worldY = (avgH/255.0f) * HEIGHT_SCALE;
    int tile = TileFromHeight((uint8_t)avgH);
    float worldX = (bx+blockSize*0.5f)*CELL_SIZE - halfGrid;
    float worldZ = (by+blockSize*0.5f)*CELL_SIZE - halfGrid;
    float tileSize = blockSize*CELL_SIZE - GAP;
    Matrix transform = MatrixMultiply(
        MatrixScale(tileSize, 1.0f, tileSize),
        MatrixTranslate(worldX, worldY, worldZ));
    DrawMesh(planeMesh, tileMat[tile], transform);
    drawCallCount++;
}

static void DrawLODTiles(int bx, int by, int blockSize, float halfGrid, float camX, float camZ) {
    float cx = (bx+blockSize*0.5f)*CELL_SIZE - halfGrid;
    float cz = (by+blockSize*0.5f)*CELL_SIZE - halfGrid;
    float dist = sqrtf((cx-camX)*(cx-camX)+(cz-camZ)*(cz-camZ));
    if (blockSize > 1) {
        int lodIdx = (blockSize == 8) ? 2 : (blockSize == 4) ? 1 : (blockSize == 2) ? 0 : -1;
        if (lodIdx >= 0 && dist < LOD_DIST[lodIdx]) {
            int half = blockSize/2;
            DrawLODTiles(bx, by, half, halfGrid, camX, camZ);
            DrawLODTiles(bx+half, by, half, halfGrid, camX, camZ);
            DrawLODTiles(bx, by+half, half, halfGrid, camX, camZ);
            DrawLODTiles(bx+half, by+half, half, halfGrid, camX, camZ);
            return;
        }
    }
    DrawTile(bx, by, blockSize, halfGrid);
}

// ===================== SOLID LOD MESH =====================

// Sample heightmap, clamping to bounds
static float SampleHeight(int gx, int gz) {
    gx = (gx < 0) ? 0 : (gx >= MAP_SIZE ? MAP_SIZE-1 : gx);
    gz = (gz < 0) ? 0 : (gz >= MAP_SIZE ? MAP_SIZE-1 : gz);
    return (heightmap[gz * MAP_SIZE + gx] / 255.0f) * HEIGHT_SCALE;
}

// Add a quad to the solid mesh vertex arrays
static void PushSolidQuad(int bx, int by, int blockSize, float halfGrid) {
    if (solidVertCount + 6 > MAX_VERTS) return;

    // 4 corners in grid coords -> world positions
    // Use actual heightmap samples at corners for smooth terrain
    float x0 = bx * CELL_SIZE - halfGrid;
    float x1 = (bx + blockSize) * CELL_SIZE - halfGrid;
    float z0 = by * CELL_SIZE - halfGrid;
    float z1 = (by + blockSize) * CELL_SIZE - halfGrid;

    float y00 = SampleHeight(bx, by);
    float y10 = SampleHeight(bx + blockSize, by);
    float y01 = SampleHeight(bx, by + blockSize);
    float y11 = SampleHeight(bx + blockSize, by + blockSize);

    // CCW winding, normals up
    Vector3 v0 = {x0, y00, z0};
    Vector3 v1 = {x0, y01, z1};
    Vector3 v2 = {x1, y10, z0};
    Vector3 n1 = Vector3Normalize(Vector3CrossProduct(
        Vector3Subtract(v1, v0), Vector3Subtract(v2, v0)));

    Vector3 v3 = {x1, y10, z0};
    Vector3 v4 = {x0, y01, z1};
    Vector3 v5 = {x1, y11, z1};
    Vector3 n2 = Vector3Normalize(Vector3CrossProduct(
        Vector3Subtract(v4, v3), Vector3Subtract(v5, v3)));

    Vector3 verts[6] = {v0, v1, v2, v3, v4, v5};
    Vector3 norms[6] = {n1, n1, n1, n2, n2, n2};

    for (int i = 0; i < 6; i++) {
        int idx3 = (solidVertCount + i) * 3;
        solidMesh.vertices[idx3+0] = verts[i].x;
        solidMesh.vertices[idx3+1] = verts[i].y;
        solidMesh.vertices[idx3+2] = verts[i].z;
        solidMesh.normals[idx3+0] = norms[i].x;
        solidMesh.normals[idx3+1] = norms[i].y;
        solidMesh.normals[idx3+2] = norms[i].z;

        Color c = HeightColor(verts[i].y);
        int ci = (solidVertCount + i) * 4;
        solidMesh.colors[ci+0] = c.r;
        solidMesh.colors[ci+1] = c.g;
        solidMesh.colors[ci+2] = c.b;
        solidMesh.colors[ci+3] = c.a;
    }
    solidVertCount += 6;
}

// Recursive LOD traversal that pushes quads into the solid mesh
static void BuildLODSolid(int bx, int by, int blockSize, float halfGrid, float camX, float camZ) {
    float cx = (bx + blockSize * 0.5f) * CELL_SIZE - halfGrid;
    float cz = (by + blockSize * 0.5f) * CELL_SIZE - halfGrid;
    float dist = sqrtf((cx-camX)*(cx-camX) + (cz-camZ)*(cz-camZ));

    if (blockSize > 1) {
        int lodIdx = (blockSize == 8) ? 2 : (blockSize == 4) ? 1 : (blockSize == 2) ? 0 : -1;
        if (lodIdx >= 0 && dist < LOD_DIST[lodIdx]) {
            int half = blockSize / 2;
            BuildLODSolid(bx, by, half, halfGrid, camX, camZ);
            BuildLODSolid(bx+half, by, half, halfGrid, camX, camZ);
            BuildLODSolid(bx, by+half, half, halfGrid, camX, camZ);
            BuildLODSolid(bx+half, by+half, half, halfGrid, camX, camZ);
            return;
        }
    }
    PushSolidQuad(bx, by, blockSize, halfGrid);
}

static void InitSolidMesh() {
    solidMesh = {0};
    solidMesh.vertexCount = MAX_VERTS;
    solidMesh.triangleCount = MAX_QUADS * 2;
    solidMesh.vertices = (float *)MemAlloc(MAX_VERTS * 3 * sizeof(float));
    solidMesh.normals  = (float *)MemAlloc(MAX_VERTS * 3 * sizeof(float));
    solidMesh.colors   = (unsigned char *)MemAlloc(MAX_VERTS * 4 * sizeof(unsigned char));

    // Zero-init so unused verts are degenerate
    memset(solidMesh.vertices, 0, MAX_VERTS * 3 * sizeof(float));
    memset(solidMesh.normals, 0, MAX_VERTS * 3 * sizeof(float));
    memset(solidMesh.colors, 0, MAX_VERTS * 4 * sizeof(unsigned char));

    UploadMesh(&solidMesh, true); // dynamic = true for frequent updates
    solidModel = LoadModelFromMesh(solidMesh);
    solidModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    solidMeshInited = true;
}

static void RebuildSolidLODMesh(float camX, float camZ) {
    float halfGrid = MAP_SIZE * CELL_SIZE * 0.5f;
    solidVertCount = 0;

    for (int by = 0; by < MAP_SIZE; by += 8)
        for (int bx = 0; bx < MAP_SIZE; bx += 8)
            BuildLODSolid(bx, by, 8, halfGrid, camX, camZ);

    // Zero out remaining verts (degenerate triangles)
    if (solidVertCount < MAX_VERTS) {
        memset(solidMesh.vertices + solidVertCount * 3, 0,
               (MAX_VERTS - solidVertCount) * 3 * sizeof(float));
    }

    // Update GPU buffers
    UpdateMeshBuffer(solidMesh, 0, solidMesh.vertices, MAX_VERTS * 3 * sizeof(float), 0); // vertices
    UpdateMeshBuffer(solidMesh, 1, solidMesh.normals,  MAX_VERTS * 3 * sizeof(float), 0); // normals
    UpdateMeshBuffer(solidMesh, 3, solidMesh.colors,   MAX_VERTS * 4 * sizeof(unsigned char), 0); // colors
}

// ===================== EXPORTED C FUNCTIONS =====================

#ifdef __EMSCRIPTEN__
extern "C" {

EMSCRIPTEN_KEEPALIVE
int js_get_map_size() { return MAP_SIZE; }

EMSCRIPTEN_KEEPALIVE
void js_set_pixel(int x, int y, int value) {
    if (x >= 0 && x < MAP_SIZE && y >= 0 && y < MAP_SIZE)
        heightmap[y * MAP_SIZE + x] = (uint8_t)value;
}

EMSCRIPTEN_KEEPALIVE
void js_rebuild_mesh() { /* rebuilt every frame based on camera */ }

EMSCRIPTEN_KEEPALIVE
void js_set_render_mode(int mode) { renderMode = mode; }

} // extern "C"

EM_JS(void, notify_html, (const char *msg), {
    if (typeof window.updateStatusFromC === 'function')
        window.updateStatusFromC(UTF8ToString(msg));
});
#else
void notify_html(const char *) {}
#endif

// ===================== MAIN LOOP =====================

static void UpdateAndDraw() {
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        dragging = true;
        lastMouse = GetMousePosition();
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) dragging = false;
    if (dragging) {
        Vector2 cur = GetMousePosition();
        camAngle -= (cur.x - lastMouse.x) * 0.005f;
        camPitch += (cur.y - lastMouse.y) * 0.005f;
        camPitch = Clamp(camPitch, 0.1f, 1.4f);
        lastMouse = cur;
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0) camDist = Clamp(camDist - wheel * 0.8f, 2.0f, 20.0f);

    camera.position = (Vector3){
        cosf(camAngle) * cosf(camPitch) * camDist,
        sinf(camPitch) * camDist,
        sinf(camAngle) * cosf(camPitch) * camDist
    };

    float camX = camera.position.x;
    float camZ = camera.position.z;

    BeginDrawing();
    ClearBackground((Color){30, 30, 40, 255});
    BeginMode3D(camera);

    drawCallCount = 0;

    if (renderMode == 0) {
        // --- Tile LOD mode ---
        float halfGrid = MAP_SIZE * CELL_SIZE * 0.5f;
        for (int by = 0; by < MAP_SIZE; by += 8)
            for (int bx = 0; bx < MAP_SIZE; bx += 8)
                DrawLODTiles(bx, by, 8, halfGrid, camX, camZ);
    } else {
        // --- Solid LOD mesh mode ---
        RebuildSolidLODMesh(camX, camZ);
        DrawModel(solidModel, (Vector3){0,0,0}, 1.0f, WHITE);
        DrawModelWires(solidModel, (Vector3){0,0,0}, 1.0f, (Color){0,0,0,40});
        drawCallCount = solidVertCount / 6;
    }

    DrawGrid(10, 1.0f);
    EndMode3D();

    // Legend
    int lx = 8, ly = GetScreenHeight() - 28;
    const char *names[] = {"Water","Sand","Grass","Rock","Snow"};
    Color lcol[] = {{30,60,140,255},{180,160,100,255},{40,120,30,255},{100,95,90,255},{220,220,225,255}};
    for (int i = 0; i < TILE_COUNT; i++) {
        DrawRectangle(lx, ly, 12, 12, lcol[i]);
        DrawRectangleLines(lx, ly, 12, 12, WHITE);
        DrawText(names[i], lx+16, ly, 12, (Color){200,200,200,200});
        lx += MeasureText(names[i], 12) + 26;
    }

    const char *modeLabel = renderMode == 0 ? "Mode: Tiles (LOD)" : "Mode: Solid Mesh (LOD)";
    DrawText(modeLabel, 8, 8, 14, (Color){200,200,200,180});
    DrawText(TextFormat("Quads: %d", drawCallCount), 8, 26, 14, (Color){100,255,100,220});
    DrawText("Right-drag: orbit | Scroll: zoom", 8, 44, 14, (Color){150,150,150,140});
    DrawFPS(GetScreenWidth() - 90, 8);
    EndDrawing();
}

int main() {
    InitWindow(800, 600, "GenMesh Heightmap - LOD");
    SetTargetFPS(60);

    camera.target = (Vector3){0,0,0};
    camera.up = (Vector3){0,1,0};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    planeMesh = GenPlaneMesh();
    for (int i = 0; i < TILE_COUNT; i++) {
        tileTex[i] = GenTile(i);
        tileMat[i] = LoadMaterialDefault();
        tileMat[i].maps[MATERIAL_MAP_DIFFUSE].texture = tileTex[i];
        tileMat[i].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    }
    InitSolidMesh();

#ifdef __EMSCRIPTEN__
    notify_html("Raylib ready");
    emscripten_set_main_loop(UpdateAndDraw, 0, 1);
#else
    while (!WindowShouldClose()) UpdateAndDraw();
#endif

    UnloadMesh(planeMesh);
    for (int i = 0; i < TILE_COUNT; i++) {
        UnloadMaterial(tileMat[i]);
        UnloadTexture(tileTex[i]);
    }
    if (solidMeshInited) UnloadModel(solidModel);
    CloseWindow();
    return 0;
}
