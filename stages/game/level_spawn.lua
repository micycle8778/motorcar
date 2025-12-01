local prefixes = {
    "Wall",
    "Appliances",
    "Divider",
    -- "Food"
}

function has_prefix(s) 
    for _, p in ipairs(prefixes) do
        if string.sub(s, 1, #p) == p then
            return true
        end
    end

    return false
end

local gltf = Resources.get_gltf("level.glb")

local level = ECS.new_entity()
ECS.insert_component(level, "gltf", "level.glb")
ECS.insert_component(level, "transform", Transform.new())

for _, obj in ipairs(gltf.objects) do
    if has_prefix(obj.name) then
        local wall = ECS.new_entity()
        ECS.insert_component(wall, "transform", Transform.new())
        ECS.insert_component(wall, "parent", level)
        ECS.insert_component(wall, "body", Body.new(obj.aabb))
    end
end

function spawn_food(model_name, pos_z) 
    local x = ECS.new_entity()
    ECS.insert_component(x, "gltf", model_name)
    ECS.insert_component(x, "transform", Transform.new()
        :with_position(vec3.new(0., 1.5, pos_z))
    )
end

-- local models = {
--     "sausage_raw.glb",
--     "sausage_cooked.glb",
--     "tomato.glb",
--     "tomato_squashed.glb",
--     "hotdog.glb",
--     "hotdog_bun.glb",
--     "potato.glb",
--     "potato_squashed.glb",
--     "mashed_potatoes.glb",
--     "chili.glb",
--     "frying_pan.glb",
--     "mixing_bowl.glb",
--     "pot.glb"
-- }
--
-- for idx, name in ipairs(models) do
--     spawn_food(name, ((#models / -2) + idx) * 1.5)
-- end

-- local launcher = ECS.new_entity()
-- ECS.insert_component(launcher, "gltf", "launcher.glb")
-- ECS.insert_component(launcher, "transform", 
--     Transform.new()
--     :with_position(vec3.new(0., 4., 0.))
-- )
