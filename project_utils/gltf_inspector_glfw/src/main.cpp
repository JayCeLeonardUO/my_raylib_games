#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>

// ── State ───────────────────────────────────────────────────────────────────

static std::string gFilePath;
static cgltf_data* gData = nullptr;
static std::string gErrorMsg;

// ── Helpers ─────────────────────────────────────────────────────────────────

static const char* Safe(const char* s, const char* fallback = "(none)") {
    return (s && s[0]) ? s : fallback;
}

static const char* TypeStr(cgltf_type t) {
    switch (t) {
        case cgltf_type_scalar: return "SCALAR";
        case cgltf_type_vec2:   return "VEC2";
        case cgltf_type_vec3:   return "VEC3";
        case cgltf_type_vec4:   return "VEC4";
        case cgltf_type_mat2:   return "MAT2";
        case cgltf_type_mat3:   return "MAT3";
        case cgltf_type_mat4:   return "MAT4";
        default:                return "?";
    }
}

static const char* CompTypeStr(cgltf_component_type ct) {
    switch (ct) {
        case cgltf_component_type_r_8:   return "BYTE";
        case cgltf_component_type_r_8u:  return "UBYTE";
        case cgltf_component_type_r_16:  return "SHORT";
        case cgltf_component_type_r_16u: return "USHORT";
        case cgltf_component_type_r_32u: return "UINT";
        case cgltf_component_type_r_32f: return "FLOAT";
        default:                         return "?";
    }
}

static const char* PrimTypeStr(cgltf_primitive_type pt) {
    switch (pt) {
        case cgltf_primitive_type_points:         return "POINTS";
        case cgltf_primitive_type_lines:          return "LINES";
        case cgltf_primitive_type_line_loop:      return "LINE_LOOP";
        case cgltf_primitive_type_line_strip:     return "LINE_STRIP";
        case cgltf_primitive_type_triangles:      return "TRIANGLES";
        case cgltf_primitive_type_triangle_strip: return "TRI_STRIP";
        case cgltf_primitive_type_triangle_fan:   return "TRI_FAN";
        default:                                  return "?";
    }
}

static const char* AttrTypeStr(cgltf_attribute_type at) {
    switch (at) {
        case cgltf_attribute_type_position:   return "POSITION";
        case cgltf_attribute_type_normal:     return "NORMAL";
        case cgltf_attribute_type_tangent:    return "TANGENT";
        case cgltf_attribute_type_texcoord:   return "TEXCOORD";
        case cgltf_attribute_type_color:      return "COLOR";
        case cgltf_attribute_type_joints:     return "JOINTS";
        case cgltf_attribute_type_weights:    return "WEIGHTS";
        default:                              return "CUSTOM";
    }
}

static const char* AlphaModeStr(cgltf_alpha_mode m) {
    switch (m) {
        case cgltf_alpha_mode_opaque: return "OPAQUE";
        case cgltf_alpha_mode_mask:   return "MASK";
        case cgltf_alpha_mode_blend:  return "BLEND";
        default:                      return "?";
    }
}

static const char* InterpStr(cgltf_interpolation_type i) {
    switch (i) {
        case cgltf_interpolation_type_linear:       return "LINEAR";
        case cgltf_interpolation_type_step:         return "STEP";
        case cgltf_interpolation_type_cubic_spline: return "CUBICSPLINE";
        default:                                    return "?";
    }
}

static const char* AnimPathStr(cgltf_animation_path_type p) {
    switch (p) {
        case cgltf_animation_path_type_translation: return "translation";
        case cgltf_animation_path_type_rotation:    return "rotation";
        case cgltf_animation_path_type_scale:       return "scale";
        case cgltf_animation_path_type_weights:     return "weights";
        default:                                    return "?";
    }
}

// Index helpers — returns index of item in its parent array, or -1
static int BufIdx(cgltf_buffer* b) {
    if (!b || !gData) return -1;
    return (int)(b - gData->buffers);
}
static int BvIdx(cgltf_buffer_view* bv) {
    if (!bv || !gData) return -1;
    return (int)(bv - gData->buffer_views);
}
static int AccIdx(cgltf_accessor* a) {
    if (!a || !gData) return -1;
    return (int)(a - gData->accessors);
}
static int ImgIdx(cgltf_image* img) {
    if (!img || !gData) return -1;
    return (int)(img - gData->images);
}
static int TexIdx(cgltf_texture* tex) {
    if (!tex || !gData) return -1;
    return (int)(tex - gData->textures);
}
static int MatIdx(cgltf_material* mat) {
    if (!mat || !gData) return -1;
    return (int)(mat - gData->materials);
}
static int NodeIdx(cgltf_node* n) {
    if (!n || !gData) return -1;
    return (int)(n - gData->nodes);
}
static int MeshIdx(cgltf_mesh* m) {
    if (!m || !gData) return -1;
    return (int)(m - gData->meshes);
}

// ── File loading ────────────────────────────────────────────────────────────

static void UnloadAll() {
    if (gData) { cgltf_free(gData); gData = nullptr; }
    gErrorMsg.clear();
}

static void LoadFile(const char* path) {
    UnloadAll();
    gFilePath = path;

    cgltf_options opts = {};
    cgltf_result res = cgltf_parse_file(&opts, path, &gData);
    if (res != cgltf_result_success) {
        gErrorMsg = "cgltf_parse_file failed (code " + std::to_string((int)res) + ")";
        gData = nullptr;
        return;
    }
    res = cgltf_load_buffers(&opts, gData, path);
    if (res != cgltf_result_success) {
        gErrorMsg = "Warning: cgltf_load_buffers failed (code " + std::to_string((int)res) + "). Binary data not available.";
    }
    res = cgltf_validate(gData);
    if (res != cgltf_result_success) {
        gErrorMsg += " Validation warning (code " + std::to_string((int)res) + ").";
    }
}

// ── Accessor info (reused in multiple panels) ───────────────────────────────

static void ShowAccessorBrief(const char* label, cgltf_accessor* acc) {
    if (!acc) return;
    int ai = AccIdx(acc);
    int bvi = acc->buffer_view ? BvIdx(acc->buffer_view) : -1;
    ImGui::Text("%s: accessor[%d] %s %s x %zu",
                label, ai, TypeStr(acc->type), CompTypeStr(acc->component_type), acc->count);
    if (acc->buffer_view) {
        int bi = BufIdx(acc->buffer_view->buffer);
        ImGui::Text("    -> bufferView[%d] buffer[%d] offset=%zu size=%zu (acc offset=%zu)",
                    bvi, bi, acc->buffer_view->offset, acc->buffer_view->size, acc->offset);
    }
}

static void ShowTextureRef(const char* label, cgltf_texture_view* tv) {
    if (!tv || !tv->texture) return;
    int ti = TexIdx(tv->texture);
    int ii = tv->texture->image ? ImgIdx(tv->texture->image) : -1;
    ImGui::Text("%s: texture[%d] -> image[%d] \"%s\"",
                label, ti, ii,
                (tv->texture->image ? Safe(tv->texture->image->uri, Safe(tv->texture->image->name, "(embedded)")) : "?"));
    ImGui::Text("    texCoord=%d scale=%.3f", tv->texcoord, tv->scale);
}

// ── Panels ──────────────────────────────────────────────────────────────────

static void DrawAssetPanel() {
    if (!gData) return;
    ImGui::Begin("Asset");
    ImGui::Text("glTF Version: %s", Safe(gData->asset.version));
    ImGui::Text("Generator:    %s", Safe(gData->asset.generator));
    if (gData->asset.copyright)
        ImGui::Text("Copyright:    %s", gData->asset.copyright);
    if (gData->asset.min_version)
        ImGui::Text("Min Version:  %s", gData->asset.min_version);
    ImGui::Separator();
    ImGui::Text("File Type: %s", gData->file_type == cgltf_file_type_glb ? "GLB (binary)" : "glTF (JSON)");
    ImGui::Separator();
    ImGui::Text("Scenes:      %zu", gData->scenes_count);
    ImGui::Text("Nodes:       %zu", gData->nodes_count);
    ImGui::Text("Meshes:      %zu", gData->meshes_count);
    ImGui::Text("Materials:   %zu", gData->materials_count);
    ImGui::Text("Textures:    %zu", gData->textures_count);
    ImGui::Text("Images:      %zu", gData->images_count);
    ImGui::Text("Samplers:    %zu", gData->samplers_count);
    ImGui::Text("Animations:  %zu", gData->animations_count);
    ImGui::Text("Skins:       %zu", gData->skins_count);
    ImGui::Text("Cameras:     %zu", gData->cameras_count);
    ImGui::Text("Accessors:   %zu", gData->accessors_count);
    ImGui::Text("BufferViews: %zu", gData->buffer_views_count);
    ImGui::Text("Buffers:     %zu", gData->buffers_count);
    if (gData->extensions_used_count > 0) {
        ImGui::Separator();
        ImGui::Text("Extensions Used:");
        for (cgltf_size i = 0; i < gData->extensions_used_count; i++)
            ImGui::BulletText("%s", gData->extensions_used[i]);
    }
    if (gData->extensions_required_count > 0) {
        ImGui::Text("Extensions Required:");
        for (cgltf_size i = 0; i < gData->extensions_required_count; i++)
            ImGui::BulletText("%s", gData->extensions_required[i]);
    }
    ImGui::End();
}

static void DrawNodeTree(cgltf_node* node) {
    char label[256];
    int ni = NodeIdx(node);
    snprintf(label, sizeof(label), "[%d] %s##n%d", ni, Safe(node->name, "(unnamed)"), ni);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (node->children_count == 0) flags |= ImGuiTreeNodeFlags_Leaf;

    if (ImGui::TreeNodeEx(label, flags)) {
        if (node->has_translation)
            ImGui::Text("translation: [%.4f, %.4f, %.4f]", node->translation[0], node->translation[1], node->translation[2]);
        if (node->has_rotation)
            ImGui::Text("rotation:    [%.4f, %.4f, %.4f, %.4f]", node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);
        if (node->has_scale)
            ImGui::Text("scale:       [%.4f, %.4f, %.4f]", node->scale[0], node->scale[1], node->scale[2]);
        if (node->has_matrix) {
            ImGui::Text("matrix:");
            for (int r = 0; r < 4; r++)
                ImGui::Text("  [%.4f, %.4f, %.4f, %.4f]",
                            node->matrix[r*4+0], node->matrix[r*4+1], node->matrix[r*4+2], node->matrix[r*4+3]);
        }
        if (node->mesh)
            ImGui::Text("mesh: [%d] \"%s\"", MeshIdx(node->mesh), Safe(node->mesh->name));
        if (node->skin)
            ImGui::Text("skin: [%d] \"%s\"", (int)(node->skin - gData->skins), Safe(node->skin->name));
        if (node->camera)
            ImGui::Text("camera: [%d] \"%s\"", (int)(node->camera - gData->cameras), Safe(node->camera->name));
        if (node->weights_count > 0) {
            ImGui::Text("weights: %zu values", node->weights_count);
        }

        for (cgltf_size i = 0; i < node->children_count; i++)
            DrawNodeTree(node->children[i]);
        ImGui::TreePop();
    }
}

static void DrawScenesPanel() {
    if (!gData) return;
    ImGui::Begin("Scenes & Nodes");
    for (cgltf_size s = 0; s < gData->scenes_count; s++) {
        cgltf_scene* scene = &gData->scenes[s];
        bool isDefault = (gData->scene == scene);
        char label[128];
        snprintf(label, sizeof(label), "Scene [%zu] \"%s\"%s", s, Safe(scene->name), isDefault ? " (default)" : "");
        if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
            for (cgltf_size n = 0; n < scene->nodes_count; n++)
                DrawNodeTree(scene->nodes[n]);
        }
    }
    ImGui::End();
}

static void DrawMeshesPanel() {
    if (!gData) return;
    ImGui::Begin("Meshes");

    for (cgltf_size m = 0; m < gData->meshes_count; m++) {
        cgltf_mesh* mesh = &gData->meshes[m];
        char label[128];
        snprintf(label, sizeof(label), "Mesh [%zu] \"%s\" (%zu primitives)##m%zu",
                 m, Safe(mesh->name), mesh->primitives_count, m);

        if (ImGui::CollapsingHeader(label)) {
            if (mesh->weights_count > 0)
                ImGui::Text("Morph target weights: %zu", mesh->weights_count);

            for (cgltf_size p = 0; p < mesh->primitives_count; p++) {
                cgltf_primitive* prim = &mesh->primitives[p];
                ImGui::PushID((int)(m * 1000 + p));

                if (ImGui::TreeNode("Primitive", "Primitive [%zu] type=%s", p, PrimTypeStr(prim->type))) {
                    // Material ref
                    if (prim->material) {
                        ImGui::Text("material: [%d] \"%s\"", MatIdx(prim->material), Safe(prim->material->name));
                    } else {
                        ImGui::TextDisabled("material: (none)");
                    }

                    // Indices — binary location
                    if (prim->indices) {
                        ImGui::Separator();
                        ShowAccessorBrief("indices", prim->indices);
                    }

                    // Attributes — binary locations
                    ImGui::Separator();
                    ImGui::Text("Attributes (%zu):", prim->attributes_count);
                    for (cgltf_size a = 0; a < prim->attributes_count; a++) {
                        cgltf_attribute* attr = &prim->attributes[a];
                        char attrLabel[64];
                        snprintf(attrLabel, sizeof(attrLabel), "%s_%d", AttrTypeStr(attr->type), attr->index);
                        ShowAccessorBrief(attrLabel, attr->data);
                    }

                    // Morph targets
                    if (prim->targets_count > 0) {
                        ImGui::Separator();
                        ImGui::Text("Morph Targets: %zu", prim->targets_count);
                        for (cgltf_size t = 0; t < prim->targets_count; t++) {
                            if (ImGui::TreeNode("Target", "Target [%zu] (%zu attrs)", t, prim->targets[t].attributes_count)) {
                                for (cgltf_size ta = 0; ta < prim->targets[t].attributes_count; ta++) {
                                    cgltf_attribute* tattr = &prim->targets[t].attributes[ta];
                                    char taLabel[64];
                                    snprintf(taLabel, sizeof(taLabel), "%s_%d", AttrTypeStr(tattr->type), tattr->index);
                                    ShowAccessorBrief(taLabel, tattr->data);
                                }
                                ImGui::TreePop();
                            }
                        }
                    }

                    if (prim->has_draco_mesh_compression)
                        ImGui::TextColored(ImVec4(1,0.6f,0,1), "[Draco compressed]");

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
    }

    ImGui::End();
}

static void DrawMaterialsPanel() {
    if (!gData) return;
    ImGui::Begin("Materials");

    for (cgltf_size m = 0; m < gData->materials_count; m++) {
        cgltf_material* mat = &gData->materials[m];
        char label[128];
        snprintf(label, sizeof(label), "Material [%zu] \"%s\"##mat%zu", m, Safe(mat->name), m);

        if (ImGui::CollapsingHeader(label)) {
            ImGui::Text("alphaMode:   %s", AlphaModeStr(mat->alpha_mode));
            if (mat->alpha_mode == cgltf_alpha_mode_mask)
                ImGui::Text("alphaCutoff: %.4f", mat->alpha_cutoff);
            ImGui::Text("doubleSided: %s", mat->double_sided ? "true" : "false");
            ImGui::Text("unlit:       %s", mat->unlit ? "true" : "false");

            if (mat->has_pbr_metallic_roughness) {
                ImGui::Separator();
                ImGui::Text("pbrMetallicRoughness:");
                auto& pbr = mat->pbr_metallic_roughness;
                ImGui::Text("  baseColorFactor: [%.4f, %.4f, %.4f, %.4f]",
                            pbr.base_color_factor[0], pbr.base_color_factor[1],
                            pbr.base_color_factor[2], pbr.base_color_factor[3]);
                ImGui::Text("  metallicFactor:  %.4f", pbr.metallic_factor);
                ImGui::Text("  roughnessFactor: %.4f", pbr.roughness_factor);
                ShowTextureRef("  baseColorTexture", &pbr.base_color_texture);
                ShowTextureRef("  metallicRoughnessTexture", &pbr.metallic_roughness_texture);
            }

            if (mat->has_pbr_specular_glossiness) {
                ImGui::Separator();
                ImGui::Text("pbrSpecularGlossiness:");
                auto& sg = mat->pbr_specular_glossiness;
                ImGui::Text("  diffuseFactor:    [%.4f, %.4f, %.4f, %.4f]",
                            sg.diffuse_factor[0], sg.diffuse_factor[1], sg.diffuse_factor[2], sg.diffuse_factor[3]);
                ImGui::Text("  specularFactor:   [%.4f, %.4f, %.4f]",
                            sg.specular_factor[0], sg.specular_factor[1], sg.specular_factor[2]);
                ImGui::Text("  glossinessFactor: %.4f", sg.glossiness_factor);
            }

            ShowTextureRef("normalTexture", &mat->normal_texture);
            ShowTextureRef("occlusionTexture", &mat->occlusion_texture);
            ShowTextureRef("emissiveTexture", &mat->emissive_texture);
            ImGui::Text("emissiveFactor: [%.4f, %.4f, %.4f]",
                        mat->emissive_factor[0], mat->emissive_factor[1], mat->emissive_factor[2]);
        }
    }

    ImGui::End();
}

static void DrawAnimationsPanel() {
    if (!gData) return;
    ImGui::Begin("Animations");

    for (cgltf_size a = 0; a < gData->animations_count; a++) {
        cgltf_animation* anim = &gData->animations[a];
        char label[128];
        snprintf(label, sizeof(label), "Animation [%zu] \"%s\" (%zu ch, %zu samp)##a%zu",
                 a, Safe(anim->name), anim->channels_count, anim->samplers_count, a);

        if (ImGui::CollapsingHeader(label)) {
            float duration = 0.0f;
            for (cgltf_size s = 0; s < anim->samplers_count; s++) {
                cgltf_accessor* input = anim->samplers[s].input;
                if (input && input->has_max)
                    duration = fmaxf(duration, input->max[0]);
            }
            ImGui::Text("Duration: %.4f s", duration);

            ImGui::Separator();
            ImGui::Text("Samplers:");
            for (cgltf_size s = 0; s < anim->samplers_count; s++) {
                cgltf_animation_sampler* samp = &anim->samplers[s];
                ImGui::Text("  [%zu] interpolation=%s  input=accessor[%d] (%zu keys)  output=accessor[%d]",
                            s, InterpStr(samp->interpolation),
                            AccIdx(samp->input), samp->input ? samp->input->count : 0,
                            AccIdx(samp->output));
            }

            ImGui::Separator();
            ImGui::Text("Channels:");
            for (cgltf_size c = 0; c < anim->channels_count; c++) {
                cgltf_animation_channel* ch = &anim->channels[c];
                int sampIdx = ch->sampler ? (int)(ch->sampler - anim->samplers) : -1;
                ImGui::Text("  [%zu] node[%d] \"%s\"  path=%s  sampler=%d",
                            c, NodeIdx(ch->target_node),
                            ch->target_node ? Safe(ch->target_node->name) : "?",
                            AnimPathStr(ch->target_path), sampIdx);
            }
        }
    }

    ImGui::End();
}

static void DrawTexturesPanel() {
    if (!gData) return;
    ImGui::Begin("Textures & Images");

    if (ImGui::CollapsingHeader("Images", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (cgltf_size i = 0; i < gData->images_count; i++) {
            cgltf_image* img = &gData->images[i];
            ImGui::Text("[%zu] name=\"%s\"  uri=\"%s\"  mimeType=\"%s\"",
                        i, Safe(img->name), Safe(img->uri, "(embedded)"), Safe(img->mime_type));
            if (img->buffer_view) {
                int bvi = BvIdx(img->buffer_view);
                int bi = BufIdx(img->buffer_view->buffer);
                ImGui::Text("     -> bufferView[%d] buffer[%d] offset=%zu size=%zu",
                            bvi, bi, img->buffer_view->offset, img->buffer_view->size);
            }
        }
    }

    if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (cgltf_size t = 0; t < gData->textures_count; t++) {
            cgltf_texture* tex = &gData->textures[t];
            int ii = tex->image ? ImgIdx(tex->image) : -1;
            int si = tex->sampler ? (int)(tex->sampler - gData->samplers) : -1;
            ImGui::Text("[%zu] name=\"%s\"  image=[%d]  sampler=[%d]",
                        t, Safe(tex->name), ii, si);
        }
    }

    if (gData->samplers_count > 0 && ImGui::CollapsingHeader("Samplers")) {
        for (cgltf_size s = 0; s < gData->samplers_count; s++) {
            cgltf_sampler* samp = &gData->samplers[s];
            ImGui::Text("[%zu] name=\"%s\"  magFilter=%d  minFilter=%d  wrapS=%d  wrapT=%d",
                        s, Safe(samp->name), samp->mag_filter, samp->min_filter,
                        samp->wrap_s, samp->wrap_t);
        }
    }

    ImGui::End();
}

static void DrawSkinsPanel() {
    if (!gData || gData->skins_count == 0) return;
    ImGui::Begin("Skins");

    for (cgltf_size s = 0; s < gData->skins_count; s++) {
        cgltf_skin* skin = &gData->skins[s];
        char label[128];
        snprintf(label, sizeof(label), "Skin [%zu] \"%s\" (%zu joints)##sk%zu",
                 s, Safe(skin->name), skin->joints_count, s);

        if (ImGui::CollapsingHeader(label)) {
            if (skin->skeleton)
                ImGui::Text("skeleton: node[%d] \"%s\"", NodeIdx(skin->skeleton), Safe(skin->skeleton->name));
            if (skin->inverse_bind_matrices)
                ShowAccessorBrief("inverseBindMatrices", skin->inverse_bind_matrices);

            ImGui::Separator();
            for (cgltf_size j = 0; j < skin->joints_count; j++) {
                ImGui::Text("  joint[%zu]: node[%d] \"%s\"",
                            j, NodeIdx(skin->joints[j]), Safe(skin->joints[j]->name));
            }
        }
    }

    ImGui::End();
}

static void DrawAccessorsPanel() {
    if (!gData) return;
    ImGui::Begin("Accessors");

    for (cgltf_size a = 0; a < gData->accessors_count; a++) {
        cgltf_accessor* acc = &gData->accessors[a];
        int bvi = acc->buffer_view ? BvIdx(acc->buffer_view) : -1;

        ImGui::Text("[%zu] type=%s component=%s count=%zu offset=%zu normalized=%s",
                    a, TypeStr(acc->type), CompTypeStr(acc->component_type),
                    acc->count, acc->offset, acc->normalized ? "true" : "false");
        if (bvi >= 0)
            ImGui::Text("     bufferView=[%d]", bvi);
        if (acc->has_min || acc->has_max) {
            int nc = (int)cgltf_num_components(acc->type);
            if (acc->has_min) {
                ImGui::Text("     min: [");
                ImGui::SameLine();
                for (int c = 0; c < nc; c++) { ImGui::SameLine(); ImGui::Text("%.4f%s", acc->min[c], c < nc-1 ? "," : ""); }
                ImGui::SameLine(); ImGui::Text("]");
            }
            if (acc->has_max) {
                ImGui::Text("     max: [");
                ImGui::SameLine();
                for (int c = 0; c < nc; c++) { ImGui::SameLine(); ImGui::Text("%.4f%s", acc->max[c], c < nc-1 ? "," : ""); }
                ImGui::SameLine(); ImGui::Text("]");
            }
        }
        if (acc->is_sparse)
            ImGui::TextColored(ImVec4(1,0.6f,0,1), "     [SPARSE] count=%zu", acc->sparse.count);
    }

    ImGui::End();
}

static void DrawBufferViewsPanel() {
    if (!gData) return;
    ImGui::Begin("BufferViews");

    for (cgltf_size bv = 0; bv < gData->buffer_views_count; bv++) {
        cgltf_buffer_view* view = &gData->buffer_views[bv];
        int bi = BufIdx(view->buffer);
        const char* targetStr = "NONE";
        if (view->type == cgltf_buffer_view_type_vertices) targetStr = "ARRAY_BUFFER (vertices)";
        else if (view->type == cgltf_buffer_view_type_indices) targetStr = "ELEMENT_ARRAY_BUFFER (indices)";

        ImGui::Text("[%zu] buffer=[%d] offset=%zu size=%zu stride=%zu target=%s",
                    bv, bi, view->offset, view->size, view->stride, targetStr);
    }

    ImGui::End();
}

static void DrawBuffersPanel() {
    if (!gData) return;
    ImGui::Begin("Buffers");

    for (cgltf_size b = 0; b < gData->buffers_count; b++) {
        cgltf_buffer* buf = &gData->buffers[b];
        ImGui::Text("[%zu] size=%zu  uri=\"%s\"  data=%s",
                    b, buf->size, Safe(buf->uri, "(GLB binary chunk)"),
                    buf->data ? "loaded" : "NOT loaded");
    }

    ImGui::End();
}

static void DrawCamerasPanel() {
    if (!gData || gData->cameras_count == 0) return;
    ImGui::Begin("Cameras");

    for (cgltf_size c = 0; c < gData->cameras_count; c++) {
        cgltf_camera* cam = &gData->cameras[c];
        char label[128];
        snprintf(label, sizeof(label), "Camera [%zu] \"%s\"##cam%zu", c, Safe(cam->name), c);

        if (ImGui::CollapsingHeader(label)) {
            if (cam->type == cgltf_camera_type_perspective) {
                ImGui::Text("type: perspective");
                ImGui::Text("  yfov:        %.4f", cam->data.perspective.yfov);
                ImGui::Text("  aspectRatio: %.4f", cam->data.perspective.aspect_ratio);
                ImGui::Text("  znear:       %.4f", cam->data.perspective.znear);
                ImGui::Text("  zfar:        %.4f", cam->data.perspective.zfar);
            } else if (cam->type == cgltf_camera_type_orthographic) {
                ImGui::Text("type: orthographic");
                ImGui::Text("  xmag:  %.4f", cam->data.orthographic.xmag);
                ImGui::Text("  ymag:  %.4f", cam->data.orthographic.ymag);
                ImGui::Text("  znear: %.4f", cam->data.orthographic.znear);
                ImGui::Text("  zfar:  %.4f", cam->data.orthographic.zfar);
            }
        }
    }

    ImGui::End();
}

// ── GLFW drop callback ─────────────────────────────────────────────────────

static void DropCallback(GLFWwindow* /*window*/, int count, const char** paths) {
    if (count > 0) LoadFile(paths[0]);
}

// ── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "glTF Inspector", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetDropCallback(window, DropCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    if (argc > 1) LoadFile(argv[1]);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Full-window dockspace
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        // Main menu
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Quit", "Ctrl+Q")) glfwSetWindowShouldClose(window, true);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Status bar
        if (!gData && gFilePath.empty()) {
            ImGui::Begin("Welcome");
            ImGui::Text("Drag & drop a .gltf or .glb file, or pass one as a CLI argument.");
            ImGui::End();
        }

        if (!gErrorMsg.empty()) {
            ImGui::Begin("Status");
            ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "%s", gErrorMsg.c_str());
            ImGui::End();
        }

        if (gData) {
            ImGui::Begin("File");
            ImGui::Text("%s", gFilePath.c_str());
            ImGui::End();

            DrawAssetPanel();
            DrawScenesPanel();
            DrawMeshesPanel();
            DrawMaterialsPanel();
            DrawAnimationsPanel();
            DrawTexturesPanel();
            DrawSkinsPanel();
            DrawCamerasPanel();
            DrawAccessorsPanel();
            DrawBufferViewsPanel();
            DrawBuffersPanel();
        }

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    UnloadAll();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
