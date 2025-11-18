local clock = 0
ECS.register_system({ "transform", "sprite3d" }, function(horse) 
    clock = clock + Engine.delta()
    horse.transform:rotate_by(vec3.new(0, 1, 0), Engine.delta())
    horse.transform.position.y = 4 + math.sin(clock)
end)
