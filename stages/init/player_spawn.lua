local camera_holder_holder = ECS.new_entity()
ECS.insert_component(camera_holder_holder, "player", {})
ECS.insert_component(camera_holder_holder, "camera_holder_holder", {})
ECS.insert_component(camera_holder_holder, "transform", Transform.new():with_position(vec3.new(0, 3, 0)))
ECS.insert_component(camera_holder_holder, "body", Body.new(AABB.new(vec3.new(), vec3.new(3))))
ECS.insert_component(camera_holder_holder, "kinematic_body", {})

local camera_holder = ECS.new_entity()
ECS.insert_component(camera_holder, "camera_holder", {})
ECS.insert_component(camera_holder, "transform", Transform.new())
ECS.insert_component(camera_holder, "parent", camera_holder_holder)

local camera = ECS.new_entity()
ECS.insert_component(camera, "camera", {})
ECS.insert_component(camera, "transform", Transform.new())
ECS.insert_component(camera, "parent", camera_holder)

local horse = ECS.new_entity()
Resources.get_gltf("horse.glb").mesh_bundles[2].should_draw = false
ECS.insert_component(horse, "gltf", "horse.glb")
ECS.insert_component(horse, "parent", camera_holder_holder)
ECS.insert_component(horse, "transform", Transform.new():with_position(vec3.new(0, -1.25, 0)):rotated(vec3.new(0, 1, 0), 3.1415))

local crosshair = ECS.new_entity()
ECS.insert_component(crosshair, "transform", Transform.new():with_scale(vec3.new(2)))
ECS.insert_component(crosshair, "sprite", "insect.png")

local speed_boost = ECS.new_entity()
ECS.insert_component(speed_boost, "speed_boost", { value = 0 })

local health = ECS.new_entity()
ECS.insert_component(health, "health", { health = 5 })

Input.lock_mouse()
