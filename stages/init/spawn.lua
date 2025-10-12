local positions = { 
    vec3.new(3, 0, 0),
    vec3.new(-3, 0, 0),
    vec3.new(0, 3, 0),
    vec3.new(0, -3, 0),
    vec3.new(0, 0, 3),
    vec3.new(0, 0, -3),
}

local horse_gltf = Resources.get_gltf("horse.glb")
local aabb

for idx, mesh in ipairs(horse_gltf.mesh_bundles) do
    Log.trace(mesh.name)
    if mesh.name == "Cube" then
        mesh.should_draw = false
        aabb = mesh.aabb
    end
end

for idx, pos in ipairs(positions) do
    local horse = ECS.new_entity()
    ECS.insert_component(horse, "horse", {})
    ECS.insert_component(horse, "gltf", "horse.glb")
    ECS.insert_component(horse, "transform", Transform.new():with_position(pos))
    ECS.insert_component(horse, "body", Body.new(aabb))
end

-- for idx, pos in ipairs(positions) do
--     local cube = ECS.new_entity()
--     ECS.insert_component(cube, "cube", {})
--     ECS.insert_component(cube, "gltf", "cube.glb")
--     ECS.insert_component(cube, "transform", Transform.new():with_position(pos))
--     ECS.insert_component(cube, "body", Body.new(AABB.new()))
-- end

local camera_holder = ECS.new_entity()
ECS.insert_component(camera_holder, "camera_holder", {})
ECS.insert_component(camera_holder, "transform", Transform.new())

local camera = ECS.new_entity()
ECS.insert_component(camera, "camera", {})
ECS.insert_component(camera, "transform", Transform.new())
ECS.insert_component(camera, "parent", camera_holder)

local insect = ECS.new_entity()
ECS.insert_component(insect, "transform", Transform.new():with_scale(vec3.new(2)))
ECS.insert_component(insect, "sprite", "insect.png")

Input.lock_mouse()
