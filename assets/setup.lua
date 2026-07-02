-- -- load models
-- load_model("assets/test_glb_output/16x16 Icon1.glb", "player_model")
-- load_model("assets/test_glb_output/16x16 Icon2.glb", "pickup1")
-- load_model("assets/test_glb_output/16x16 Icon3.glb", "pickup2")
-- load_model("assets/test_glb_output/16x16 Icon4.glb", "pickup3")
-- load_model("assets/test_glb_output/16x16 Icon5.glb", "pickup4")
-- load_model("assets/test_glb_output/card_frame_back_yellow_nobg.glb", "card_frame")
--
-- -- spawn player (entity index 0)
-- spawn("player_model", 0, 0, 0)
-- --trait_add(0, "wsad")
-- trait_add(0, "is_grid_aligned")
-- trait_add(0, "is_pushable")
-- set_flag(0, "is_draggable", true)
--
-- -- spawn pickups scattered around
-- spawn("pickup1", 3, 0, 2)
-- trait_add(1, "pickup")
-- trait_add(1, "is_pushable")
--
-- spawn("pickup2", -4, 0, 1)
-- trait_add(2, "pickup")
-- trait_add(2, "is_pushable")
--
-- spawn("pickup3", 2, 0, -3)
-- trait_add(3, "pickup")
-- trait_add(3, "is_pushable")
--
-- spawn("pickup4", -2, 0, -4)
-- trait_add(4, "pickup")
-- trait_add(4, "is_pushable")
--
-- spawn("pickup1", 5, 0, -1)
-- trait_add(5, "pickup")
-- trait_add(5, "is_pushable")
--
-- spawn("pickup3", -3, 0, 4)
-- trait_add(6, "pickup")
-- trait_add(6, "is_pushable")
--
-- -- register and spawn a looping light animation
-- register_animation("light_059", "assets/animations/light_059", 0.1)
-- spawn_animation("light_059", 1, 0.5, 1, 1.0, true)
--
-- -- click anywhere to play light_059 animation at that spot
-- set_click_animation("light_059")
--
-- -- spawn card frame
-- -- spawn_child(0, "card_frame", 0, 0, 0) -- spawns card_frame as a child of entity 0
-- -- trait_add(7, "is_billboard")
-- -- set_flag(0, "is_collidable", false)
-- -- console_print("Lua setup complete! WASD to move, walk into pickups to collect them.")
-- ============================================================
-- seed planting board — static layout (primitives, hardcoded)
-- 8x8 board centered on origin, cells span ~ -3.5 .. 3.5
-- ============================================================
-- ============================================================
-- seed planting board -- static layout (primitives, hardcoded)
-- 8x8 board centered on origin, cells span ~ -3.5 .. 3.5
--
-- REQUIRES: spawn() returning an entity index (the l_spawn edit).
-- As your code sits now spawn() returns a bool, so id_board would
-- be `true` and set_flag(true, ...) fails. Apply that edit first.
-- ============================================================

-- -- load primitives (one model per color, since color() tints the shared model)
-- load_primitive("board", "plane", 8, 8, 1)
-- load_primitive("deck_card", "cube", 0.7, 0.12, 1.0)
-- load_primitive("hand_card", "cube", 0.8, 1.2, 0.1)
-- load_primitive("flower_red", "sphere", 0.35, 16, 16)
-- load_primitive("flower_cyan", "sphere", 0.35, 16, 16)
-- load_primitive("flower_gold", "sphere", 0.35, 16, 16)
-- load_primitive("flower_purp", "sphere", 0.35, 16, 16)
-- load_primitive("cross", "sphere", 0.15, 12, 12)
--
-- -- colors lifted from the demo palette
-- color("board", 22, 33, 62)
-- color("deck_card", 78, 205, 196)
-- color("hand_card", 15, 52, 96)
-- color("flower_red", 255, 107, 107)
-- color("flower_cyan", 78, 205, 196)
-- color("flower_gold", 255, 217, 61)
-- color("flower_purp", 167, 139, 250)
-- color("cross", 233, 69, 96)
--
-- -- board (capture its id so we can stop it highlighting on hover)
-- id_board = spawn("board", 0, -0.04, 0)
-- set_flag(id_board, "is_highlightable", false)
--
-- -- seed deck pile (back-left corner) -- 3 flat cards stacked with slight offset
-- spawn("deck_card", -3.0, 0.06, -3.0)
-- spawn("deck_card", -2.95, 0.18, -2.95)
-- spawn("deck_card", -2.90, 0.30, -2.90)
--
-- -- hand row (front edge, toward camera) -- 3 upright cards
-- spawn("hand_card", -1.5, 0.60, 5.0)
-- spawn("hand_card", 0.0, 0.60, 5.0)
-- spawn("hand_card", 1.5, 0.60, 5.0)
--
-- -- planted flowers (sample, on grid cells)
-- spawn("flower_red", 1, 0.35, 0)
-- spawn("flower_cyan", -2, 0.35, 2)
-- spawn("flower_gold", 2, 0.35, -1)
-- spawn("flower_purp", 0, 0.35, 2)
--
-- -- cross targets (sample, where two lines would intersect)
-- spawn("cross", -0.5, 0.15, 1.0)
-- spawn("cross", 1.5, 0.15, -0.5)
--
-- console_print("Seed board layout loaded.")

-- ============================================================
-- seed deck group test
-- builds the seed_deck group via the composable primitives:
--   spawn anchor -> init_group -> (spawn / add_to_group) loop
-- ============================================================

-- ============================================================
-- seed deck group test
-- builds the seed_deck group with the composable primitives:
--   spawn anchor -> init_group -> (spawn / add_to_group) loop
--
-- Lua owns seed position: each seed is spawned at its pile spot.
-- The seed-deck sweep pass must NOT overwrite s.position for this
-- to stay visible (drop the gx/gy write in that pass).
-- ============================================================

-- ============================================================
-- seed test -- raw dogged: no groups, no lookups.
-- direct numerical addressing: entity index == spawn order,
-- starting at 0. spawn's return value is ignored on purpose.
-- ============================================================

load_primitive("seed", "sphere", 0.25, 12, 12)
color("seed", 78, 205, 196)

-- 16 seeds in a 4x4 grid on the ground. spawn order => index 0..15.
spawn("seed", -3, 0.25, -3) -- 0
spawn("seed", -1, 0.25, -3) -- 1
spawn("seed", 1, 0.25, -3) -- 2
spawn("seed", 3, 0.25, -3) -- 3
spawn("seed", -3, 0.25, -1) -- 4
spawn("seed", -1, 0.25, -1) -- 5
spawn("seed", 1, 0.25, -1) -- 6
spawn("seed", 3, 0.25, -1) -- 7
spawn("seed", -3, 0.25, 1) -- 8
spawn("seed", -1, 0.25, 1) -- 9
spawn("seed", 1, 0.25, 1) -- 10
spawn("seed", 3, 0.25, 1) -- 11
spawn("seed", -3, 0.25, 3) -- 12
spawn("seed", -1, 0.25, 3) -- 13
spawn("seed", 1, 0.25, 3) -- 14
spawn("seed", 3, 0.25, 3) -- 15

-- direct numerical addressing test: turn off collision on a few by literal index
set_flag(0, "is_collidable", false)
set_flag(5, "is_collidable", false)
set_flag(10, "is_collidable", false)
set_flag(15, "is_collidable", false)

console_print("16 seeds spawned, addressed 0..15")
