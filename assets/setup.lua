-- load models
load_model("assets/test_glb_output/16x16 Icon1.glb", "player_model")
load_model("assets/test_glb_output/16x16 Icon2.glb", "pickup1")
load_model("assets/test_glb_output/16x16 Icon3.glb", "pickup2")
load_model("assets/test_glb_output/16x16 Icon4.glb", "pickup3")
load_model("assets/test_glb_output/16x16 Icon5.glb", "pickup4")
load_model("assets/test_glb_output/card_frame_back_yellow_nobg.glb", "card_frame")

-- spawn player (entity index 0)
spawn("player_model", 0, 0, 0)
trait_add(0, "wsad")
trait_add(0, "is_grid_aligned")
trait_add(0, "is_pushable")
set_flag(0, "is_draggable", true)

-- spawn pickups scattered around
spawn("pickup1", 3, 0, 2)
trait_add(1, "pickup")
trait_add(1, "is_pushable")

spawn("pickup2", -4, 0, 1)
trait_add(2, "pickup")
trait_add(2, "is_pushable")

spawn("pickup3", 2, 0, -3)
trait_add(3, "pickup")
trait_add(3, "is_pushable")

spawn("pickup4", -2, 0, -4)
trait_add(4, "pickup")
trait_add(4, "is_pushable")

spawn("pickup1", 5, 0, -1)
trait_add(5, "pickup")
trait_add(5, "is_pushable")

spawn("pickup3", -3, 0, 4)
trait_add(6, "pickup")
trait_add(6, "is_pushable")

-- spawn card frame
spawn_child(0, "card_frame", 0, 0, 0) -- spawns card_frame as a child of entity 0
trait_add(7, "is_billboard")

console_print("Lua setup complete! WASD to move, walk into pickups to collect them.")
