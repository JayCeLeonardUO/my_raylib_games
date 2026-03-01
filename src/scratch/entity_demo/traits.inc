
// ============================================================================
// Trait: highlightable
// ============================================================================

/** @brief Initialize the highlightable trait (no-op). */
void highlightable_init(void* ptr) {
  (void)ptr;
}

/** @brief Per-frame update for the highlightable trait (no-op). */
void highlightable_update(void* ptr) {
  (void)ptr;
}

// ============================================================================
// Trait: wsad
// ============================================================================
/** @brief Initialize the WSAD movement trait (no-op). */
void wsad_init(void* ptr) {
  (void)ptr;
}

/**
 * @brief Per-frame update for WSAD movement: moves entity on XZ plane via WASD keys.
 * @param ptr Pointer to the owning Entity.
 */
void wsad_update(void* ptr) {
  if (ImGui::GetIO().WantCaptureKeyboard)
    return;
  auto& e = *(Entity*)ptr;
  float speed = 5.0f * GetFrameTime();
  if (IsKeyDown(KEY_W))
    e.position.z -= speed;
  if (IsKeyDown(KEY_S))
    e.position.z += speed;
  if (IsKeyDown(KEY_A))
    e.position.x -= speed;
  if (IsKeyDown(KEY_D))
    e.position.x += speed;
}

// ============================================================================
// Trait: pickup
// ============================================================================
/** @brief Initialize the pickup trait (no-op). */
void pickup_init(void* ptr) {
  (void)ptr;
}

/** @brief Per-frame update for the pickup trait (no-op). */
void pickup_update(void* ptr) {
  (void)ptr;
}

/** @brief Per-frame update for the is_text trait (no-op). */
void is_text_update(void* ptr) {
  (void)ptr;
}
