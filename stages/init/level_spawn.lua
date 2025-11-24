local prefixes = {
    "Wall",
    "Appliances",
    "Divider",
    "Food"
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
