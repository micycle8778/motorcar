local positions = { 
    vec3.new(3, 0, 0),
    vec3.new(-3, 0, 0),
    vec3.new(0, 3, 0),
    vec3.new(0, -3, 0),
    vec3.new(0, 0, 3),
    vec3.new(0, 0, -3),
}

for idx, pos in ipairs(positions) do
    local horse = ECS.new_entity()
    ECS.insert_component(horse, "horse", {})
    ECS.insert_component(horse, "gltf", "horse.glb")
    ECS.insert_component(horse, "transform", Transform.new():with_position(pos))
end

local camera_holder = ECS.new_entity()
ECS.insert_component(camera_holder, "camera_holder", {})
ECS.insert_component(camera_holder, "transform", Transform.new())

local camera = ECS.new_entity()
ECS.insert_component(camera, "camera", {})
ECS.insert_component(camera, "transform", Transform.new())
ECS.insert_component(camera, "parent", camera_holder)

Input.lock_mouse()
