#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <string>
#include <dirent.h>
#include <sys/stat.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

enum FileType { FILE_TEXT, FILE_GLTF };

struct LoadedFile {
    std::string name;
    std::string path;
    FileType type;
    std::string content;
    std::vector<uint8_t> binary;
    cgltf_data *gltf = nullptr;
};

static std::vector<LoadedFile> files;
static int selectedFile = -1;
static char filterBuf[128] = "";

static bool IsGltfExt(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    return strcmp(dot, ".gltf") == 0 || strcmp(dot, ".glb") == 0;
}

static bool IsViewableExt(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    const char *exts[] = {
        ".txt", ".md", ".cpp", ".h", ".hpp", ".c", ".js", ".ts", ".py",
        ".json", ".xml", ".html", ".css", ".yaml", ".yml", ".toml", ".ini",
        ".cfg", ".sh", ".bash", ".lua", ".rs", ".go", ".java", ".csv",
        ".log", ".cmake", ".glsl", ".vert", ".frag", ".gltf", ".glb",
        nullptr
    };
    for (int i = 0; exts[i]; i++) {
        if (strcmp(dot, exts[i]) == 0) return true;
    }
    return false;
}

static void ScanDir(const std::string &dirPath, const std::string &prefix) {
    DIR *dir = opendir(dirPath.c_str());
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.') continue;

        std::string fullPath = dirPath + "/" + ent->d_name;
        std::string relName = prefix.empty() ? ent->d_name : prefix + "/" + ent->d_name;

        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            ScanDir(fullPath, relName);
        } else if (S_ISREG(st.st_mode) && IsViewableExt(ent->d_name)) {
            LoadedFile f;
            f.name = relName;
            f.path = fullPath;

            if (IsGltfExt(ent->d_name)) {
                f.type = FILE_GLTF;
                FILE *fp = fopen(fullPath.c_str(), "rb");
                if (fp) {
                    fseek(fp, 0, SEEK_END);
                    long sz = ftell(fp);
                    fseek(fp, 0, SEEK_SET);
                    f.binary.resize(sz);
                    fread(f.binary.data(), 1, sz, fp);
                    fclose(fp);

                    cgltf_options opts = {};
                    cgltf_result res = cgltf_parse(&opts, f.binary.data(), f.binary.size(), &f.gltf);
                    if (res != cgltf_result_success) f.gltf = nullptr;
                }
            } else {
                f.type = FILE_TEXT;
                FILE *fp = fopen(fullPath.c_str(), "r");
                if (fp) {
                    fseek(fp, 0, SEEK_END);
                    long sz = ftell(fp);
                    fseek(fp, 0, SEEK_SET);
                    f.content.resize(sz);
                    sz = fread(&f.content[0], 1, sz, fp);
                    f.content.resize(sz);
                    fclose(fp);
                }
            }

            files.push_back(std::move(f));
        }
    }
    closedir(dir);
}

static const char *AccessorTypeName(cgltf_type t) {
    switch (t) {
        case cgltf_type_scalar: return "SCALAR";
        case cgltf_type_vec2:   return "VEC2";
        case cgltf_type_vec3:   return "VEC3";
        case cgltf_type_vec4:   return "VEC4";
        case cgltf_type_mat2:   return "MAT2";
        case cgltf_type_mat3:   return "MAT3";
        case cgltf_type_mat4:   return "MAT4";
        default: return "?";
    }
}

static const char *AnimPathName(cgltf_animation_path_type p) {
    switch (p) {
        case cgltf_animation_path_type_translation: return "translation";
        case cgltf_animation_path_type_rotation:    return "rotation";
        case cgltf_animation_path_type_scale:       return "scale";
        case cgltf_animation_path_type_weights:     return "weights";
        default: return "?";
    }
}

static const char *AlphaModeStr(cgltf_alpha_mode m) {
    switch (m) {
        case cgltf_alpha_mode_opaque: return "OPAQUE";
        case cgltf_alpha_mode_mask:   return "MASK";
        case cgltf_alpha_mode_blend:  return "BLEND";
        default: return "?";
    }
}

static void DrawGltfInfo(cgltf_data *data) {
    if (!data) {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Failed to parse glTF");
        return;
    }

    ImGui::Text("Asset version: %s", data->asset.version ? data->asset.version : "?");
    if (data->asset.generator)
        ImGui::Text("Generator: %s", data->asset.generator);
    ImGui::Separator();

    ImGui::Text("Scenes: %d", (int)data->scenes_count);
    ImGui::Text("Nodes: %d", (int)data->nodes_count);
    ImGui::Text("Meshes: %d", (int)data->meshes_count);
    ImGui::Text("Materials: %d", (int)data->materials_count);
    ImGui::Text("Textures: %d", (int)data->textures_count);
    ImGui::Text("Images: %d", (int)data->images_count);
    ImGui::Text("Animations: %d", (int)data->animations_count);
    ImGui::Text("Skins: %d", (int)data->skins_count);
    ImGui::Text("Cameras: %d", (int)data->cameras_count);
    ImGui::Text("Buffers: %d", (int)data->buffers_count);
    ImGui::Text("Accessors: %d", (int)data->accessors_count);
    ImGui::Separator();

    if (data->meshes_count > 0 && ImGui::TreeNode("Meshes")) {
        for (cgltf_size i = 0; i < data->meshes_count; i++) {
            cgltf_mesh *mesh = &data->meshes[i];
            const char *name = mesh->name ? mesh->name : "(unnamed)";
            if (ImGui::TreeNode((void *)(intptr_t)i, "[%d] %s — %d primitive(s)", (int)i, name, (int)mesh->primitives_count)) {
                for (cgltf_size p = 0; p < mesh->primitives_count; p++) {
                    cgltf_primitive *prim = &mesh->primitives[p];
                    if (ImGui::TreeNode((void *)(intptr_t)(i * 1000 + p), "Primitive %d — %d attributes", (int)p, (int)prim->attributes_count)) {
                        for (cgltf_size a = 0; a < prim->attributes_count; a++) {
                            cgltf_attribute *attr = &prim->attributes[a];
                            ImGui::BulletText("%s: %s [%d]", attr->name,
                                AccessorTypeName(attr->data->type), (int)attr->data->count);
                        }
                        if (prim->indices)
                            ImGui::BulletText("Indices: %d", (int)prim->indices->count);
                        if (prim->material)
                            ImGui::BulletText("Material: %s", prim->material->name ? prim->material->name : "(unnamed)");
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    if (data->materials_count > 0 && ImGui::TreeNode("Materials")) {
        for (cgltf_size i = 0; i < data->materials_count; i++) {
            cgltf_material *mat = &data->materials[i];
            const char *name = mat->name ? mat->name : "(unnamed)";
            if (ImGui::TreeNode((void *)(intptr_t)i, "[%d] %s", (int)i, name)) {
                ImGui::Text("Alpha mode: %s", AlphaModeStr(mat->alpha_mode));
                ImGui::Text("Double sided: %s", mat->double_sided ? "yes" : "no");
                if (mat->has_pbr_metallic_roughness) {
                    auto &pbr = mat->pbr_metallic_roughness;
                    ImGui::Text("PBR base color: (%.2f, %.2f, %.2f, %.2f)",
                        pbr.base_color_factor[0], pbr.base_color_factor[1],
                        pbr.base_color_factor[2], pbr.base_color_factor[3]);
                    ImGui::Text("Metallic: %.2f  Roughness: %.2f",
                        pbr.metallic_factor, pbr.roughness_factor);
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    if (data->nodes_count > 0 && ImGui::TreeNode("Nodes")) {
        for (cgltf_size i = 0; i < data->nodes_count; i++) {
            cgltf_node *node = &data->nodes[i];
            const char *name = node->name ? node->name : "(unnamed)";
            if (ImGui::TreeNode((void *)(intptr_t)i, "[%d] %s", (int)i, name)) {
                if (node->mesh)
                    ImGui::Text("Mesh: %s", node->mesh->name ? node->mesh->name : "(unnamed)");
                if (node->skin)
                    ImGui::Text("Skin: %s", node->skin->name ? node->skin->name : "(unnamed)");
                if (node->camera)
                    ImGui::Text("Camera: %s", node->camera->name ? node->camera->name : "(unnamed)");
                if (node->has_translation)
                    ImGui::Text("Translation: (%.3f, %.3f, %.3f)",
                        node->translation[0], node->translation[1], node->translation[2]);
                if (node->has_rotation)
                    ImGui::Text("Rotation: (%.3f, %.3f, %.3f, %.3f)",
                        node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);
                if (node->has_scale)
                    ImGui::Text("Scale: (%.3f, %.3f, %.3f)",
                        node->scale[0], node->scale[1], node->scale[2]);
                ImGui::Text("Children: %d", (int)node->children_count);
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    if (data->animations_count > 0 && ImGui::TreeNode("Animations")) {
        for (cgltf_size i = 0; i < data->animations_count; i++) {
            cgltf_animation *anim = &data->animations[i];
            const char *name = anim->name ? anim->name : "(unnamed)";
            if (ImGui::TreeNode((void *)(intptr_t)i, "[%d] %s — %d channel(s), %d sampler(s)",
                    (int)i, name, (int)anim->channels_count, (int)anim->samplers_count)) {
                for (cgltf_size c = 0; c < anim->channels_count; c++) {
                    cgltf_animation_channel *ch = &anim->channels[c];
                    const char *target = ch->target_node && ch->target_node->name
                        ? ch->target_node->name : "(unnamed)";
                    ImGui::BulletText("%s -> %s", AnimPathName(ch->target_path), target);
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    if (data->skins_count > 0 && ImGui::TreeNode("Skins")) {
        for (cgltf_size i = 0; i < data->skins_count; i++) {
            cgltf_skin *skin = &data->skins[i];
            const char *name = skin->name ? skin->name : "(unnamed)";
            ImGui::BulletText("[%d] %s — %d joint(s)", (int)i, name, (int)skin->joints_count);
        }
        ImGui::TreePop();
    }

    if (data->images_count > 0 && ImGui::TreeNode("Images")) {
        for (cgltf_size i = 0; i < data->images_count; i++) {
            cgltf_image *img = &data->images[i];
            const char *name = img->name ? img->name : (img->uri ? img->uri : "(embedded)");
            ImGui::BulletText("[%d] %s  mime: %s", (int)i, name,
                img->mime_type ? img->mime_type : "?");
        }
        ImGui::TreePop();
    }

    if (data->buffers_count > 0 && ImGui::TreeNode("Buffers")) {
        for (cgltf_size i = 0; i < data->buffers_count; i++) {
            cgltf_buffer *buf = &data->buffers[i];
            ImGui::BulletText("[%d] %s — %d bytes", (int)i,
                buf->uri ? buf->uri : "(embedded)", (int)buf->size);
        }
        ImGui::TreePop();
    }
}

void UpdateAndDraw() {
    BeginDrawing();
    ClearBackground(DARKGRAY);

    rlImGuiBegin();

    float winW = (float)GetScreenWidth();
    float winH = (float)GetScreenHeight();
    float listWidth = winW * 0.3f;

    // File list panel
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(listWidth, winH));
    ImGui::Begin("Files", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    ImGui::Text("(%d files)", (int)files.size());
    ImGui::InputText("Filter", filterBuf, sizeof(filterBuf));
    ImGui::Separator();

    if (files.empty()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "No files found.");
    } else {
        ImGui::BeginChild("FileList", ImVec2(0, 0), true);
        for (int i = 0; i < (int)files.size(); i++) {
            if (filterBuf[0] && !strstr(files[i].name.c_str(), filterBuf)) continue;
            bool isSelected = (selectedFile == i);
            if (ImGui::Selectable(files[i].name.c_str(), isSelected)) {
                selectedFile = i;
            }
        }
        ImGui::EndChild();
    }

    ImGui::End();

    // Content panel
    ImGui::SetNextWindowPos(ImVec2(listWidth, 0));
    ImGui::SetNextWindowSize(ImVec2(winW - listWidth, winH));
    ImGui::Begin("Content", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    if (selectedFile >= 0 && selectedFile < (int)files.size()) {
        auto &f = files[selectedFile];
        ImGui::Text("%s", f.name.c_str());
        ImGui::Separator();

        if (f.type == FILE_GLTF) {
            ImGui::BeginChild("GltfContent", ImVec2(0, 0), true);
            DrawGltfInfo(f.gltf);
            ImGui::EndChild();
        } else {
            ImGui::BeginChild("TextContent", ImVec2(0, 0), true,
                ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(f.content.c_str(),
                f.content.c_str() + f.content.size());
            ImGui::EndChild();
        }
    } else {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1), "Select a file to view its contents.");
    }

    ImGui::End();

    rlImGuiEnd();
    EndDrawing();
}

int main() {
    ScanDir("/data", "");

    InitWindow(1024, 768, "file viewer");
    SetTargetFPS(60);
    rlImGuiSetup(true);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(UpdateAndDraw, 0, 1);
#else
    while (!WindowShouldClose()) {
        UpdateAndDraw();
    }
#endif

    for (auto &f : files) {
        if (f.gltf) cgltf_free(f.gltf);
    }
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
