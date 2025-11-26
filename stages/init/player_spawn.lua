local camera_holder = ECS.new_entity()
ECS.insert_component(camera_holder, "camera_holder", {})
ECS.insert_component(camera_holder, "transform", Transform.new())
ECS.insert_component(camera_holder, "body", Body.new(AABB.new(vec3.new(0, .5, 0), vec3.new(1))))
ECS.insert_component(camera_holder, "player", {})
ECS.insert_component(camera_holder, "kinematic_body", {})
ECS.insert_component(camera_holder, "holding", "")

local camera = ECS.new_entity()
ECS.insert_component(camera, "camera", {})
ECS.insert_component(camera, "transform", Transform.new():with_position(vec3.new(0, 3, 0)))
ECS.insert_component(camera, "parent", camera_holder)

local crosshair = ECS.new_entity()
ECS.insert_component(crosshair, "transform", Transform.new():with_scale(vec3.new(.05)))
ECS.insert_component(crosshair, "sprite", "insect.png")

-- local label = ECS.new_entity()
-- ECS.insert_component(label, "transform", Transform.new():with_scale(vec3.new(1)):with_position(vec3.new(0, 0, 0)))
-- ECS.insert_component(label, "text", Text.new("ae\niou"))

Input.lock_mouse()
