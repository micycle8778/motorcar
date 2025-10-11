ECS.register_system({ "entity", "transform", "gltf" }, function(e)
    e.transform:rotate_by(vec3.new(0, 1, 0), e.entity * Engine.delta())
end)

ECS.register_system({ "transform", "cube1" }, function(e)
    e.transform.position.x = Engine.clock() % 2
end)

ECS.register_system({ "entity", "body" }, function(e)
    ECS.insert_component(e.entity, "albedo", vec3.new(1, 1, 1))
end)

ECS.register_system({ "entity", "body", "colliding_with" }, function(e)
    ECS.insert_component(e.entity, "albedo", vec3.new(1, 0, 0))
end)
