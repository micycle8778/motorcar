ECS.register_system({ "entity", "transform", "gltf" }, function(e)
    e.transform:rotate_by(vec3.new(0, 1, 0), e.entity * Engine.delta())
end)
