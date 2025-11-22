local gltf = Resources.get_gltf("level.glb")

local level = ECS.new_entity()
ECS.insert_component(level, "gltf", "level.glb")
ECS.insert_component(level, "transform", Transform.new())

for _, mesh in ipairs(gltf.mesh_bundles) do
    Log.debug(mesh.name)
end
