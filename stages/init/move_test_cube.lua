ECS.register_system({ "test_cube", "transform" }, function(cube)
    local dir = 1
    if Engine.clock() % 8 > 4 then
        dir = -1
    end

    cube.transform:translate_by(vec3.new(-1, 0, 0) * Engine.delta() * dir)
    cube.transform:rotate_by(vec3.new(0, 1, 0), Engine.delta() * 4)
end)
