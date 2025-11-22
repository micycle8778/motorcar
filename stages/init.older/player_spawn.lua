local camera_holder = ECS.new_entity()
ECS.insert_component(camera_holder, "camera_holder", {})
ECS.insert_component(camera_holder, "transform", Transform.new():with_position(vec3.new(0, 2, 0)))
ECS.insert_component(camera_holder, "body", Body.new(AABB.new(vec3.new(), vec3.new(.5))))
ECS.insert_component(camera_holder, "kinematic_body", {})

local camera = ECS.new_entity()
ECS.insert_component(camera, "camera", {})
ECS.insert_component(camera, "transform", Transform.new())
ECS.insert_component(camera, "parent", camera_holder)

local insect = ECS.new_entity()
ECS.insert_component(insect, "transform", Transform.new():with_scale(vec3.new(2)))
ECS.insert_component(insect, "sprite", "insect.png")

Input.lock_mouse()
